#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <cmath>
#include "AudioDevice.hpp"
#include "Oscillator.hpp"
#include "MidiController.hpp"

static bool progRunning = true;

static inline int16_t
clip (double x)
{
    if (x > 1.0)
        x = 1.0;
    else if (x < -1.0)
        x = -1.0;
    return static_cast<int16_t>(32767.0 * x);
}

/*
 * Catching Ctrl+C allows for all the devices to clean up safely via their
 * destructor.
 */
void
sigint (int sig_num) 
{ 
    progRunning = false;
    printf("\nExiting ...\n");
    fflush(stdout);
}

typedef enum _FilterMode {
    FILTER_LOWPASS = 0,
    FILTER_HIGHPASS,
    FILTER_BANDPASS,
} FilterMode;

/*
 * Low/Hi/Bandpass filter
 */
class Filter {
public:
    Filter (const double cutoff, const double resonance)
        : _mode (FILTER_LOWPASS)
        , _cutoff (0.0)
        , _cutoffThresh (cutoff)
        , _cutoffMod (0.0)
        , _resonance (resonance)
        , _buf0 (0.0)
        , _buf1 (0.0)
        , _buf2 (0.0)
        , _buf3 (0.0)
    {
        updateCutoff();
        updateFeedback();
    }

    double process (const double input)
    {
        if (input == 0.0)
            return input;
        _buf0 += _cutoff * (input - _buf0 + _feedback * (_buf0 - _buf1));
        _buf1 += _cutoff * (_buf0 - _buf1);
        _buf2 += _cutoff * (_buf1 - _buf2);
        _buf3 += _cutoff * (_buf2 - _buf3);
        switch (_mode) {
            case FILTER_LOWPASS:
                return _buf3;
            case FILTER_HIGHPASS:
                return input - _buf3;
            case FILTER_BANDPASS:
                return _buf0 - _buf3;
            default:
                return 0.0;
        }
    }

    void setCutoff (const double cutoff)
    {
        _cutoffThresh = cutoff;
        updateCutoff();
        updateFeedback();
    }

    void setCutoffMod (const double cutoffMod)
    {
        _cutoffMod = cutoffMod;
        updateCutoff();
        updateFeedback();
    }

    void setResonance (const double resonance)
    {
        _resonance = resonance;
        updateFeedback();
    }

    void setMode (FilterMode mode)
    {
        _mode = mode;
    }

protected:
    void inline updateCutoff ()
    {
        _cutoff = fmax(fmin(_cutoffThresh + _cutoffMod, 0.99), 0.01);
    }

    void inline updateFeedback ()
    {
        _feedback = _resonance + (_resonance / (1.0 - _cutoff));
    }

private:
    FilterMode _mode;

    /* actual cutoff used when filtering */
    double _cutoff;
    /* used as the cutoff threshold when adding the modulation */
    double _cutoffThresh;
    /* modulation from an envelope or whatever else */
    double _cutoffMod;
    double _resonance;
    double _feedback;
    /* four filter accumulators in series */
    double _buf0;
    double _buf1;
    double _buf2;
    double _buf3;
};

typedef enum _EnvelopeStage {
    STAGE_ATTACK = 0,
    STAGE_DECAY,
    STAGE_SUSTAIN,
    STAGE_RELEASE,
    NUM_STAGES,
} EnvelopeStage;

class Envelope {
public:
    /* Sample rate for all envelopes */
    static unsigned long rate;

    /* Set the sample rate for all envelopes created */
    static void setRate (unsigned long rate)
    {
        Envelope::rate = rate;
    }

    Envelope (double ADSR[4])
        : _minLevel (0.0001)
        , _level (_minLevel)
        , _multiplier (1.0) 
        , _currStage (STAGE_ATTACK)
        , _currSample (0)
        , _nextStageAt (0)
    {
        /* determines when the next stage occurs */
        _values[STAGE_ATTACK]  = ADSR[0];
        _values[STAGE_DECAY]   = ADSR[1];
        _values[STAGE_SUSTAIN] = ADSR[2];
        _values[STAGE_RELEASE] = ADSR[3];

        /* simple state transition table */
        _next[STAGE_ATTACK]  = STAGE_DECAY;
        _next[STAGE_DECAY]   = STAGE_SUSTAIN;
        _next[STAGE_SUSTAIN] = STAGE_SUSTAIN;
        _next[STAGE_RELEASE] = STAGE_RELEASE;
    }

    void noteOn ()
    {
        enterStage(STAGE_ATTACK);
    }

    void noteOff ()
    {
        enterStage(STAGE_RELEASE);
    }

    bool isActive () const
    {
        if (_currStage == STAGE_RELEASE && _level <= _minLevel)
            return false;
        return true;
    }

    double next ()
    {
        if (_currStage != STAGE_SUSTAIN) {
            if (_currSample == _nextStageAt)
                enterStage(getNextStage());
            _level *= _multiplier;
            _currSample++;
        }
        return _level;
    }

    void setValue (EnvelopeStage stage, double value)
    {
        _values[stage] = value;
        if (_currStage != stage)
            return;
        if (_currStage == STAGE_SUSTAIN) {
            _level = value;
        }
        else if (_currStage == STAGE_DECAY && stage == STAGE_SUSTAIN) {
            size_t samplesLeft = _nextStageAt - _currSample;
            calcStageMultiplier(_level, fmax(value, _minLevel), samplesLeft);
        }
        else {
            double nextLevel = _minLevel;
            switch (_currStage) {
                case STAGE_ATTACK:
                    nextLevel = 1.0;
                    break;
                case STAGE_DECAY:
                    nextLevel = fmax(_values[STAGE_SUSTAIN], _minLevel);
                    break;
                case STAGE_RELEASE:
                    nextLevel = _minLevel;
                    break;
                default:
                    break;
            }

            double percentDone = (double) _currSample / (double) _nextStageAt;
            double percentLeft = 1.0 - percentDone;
            size_t samplesLeft = percentLeft * value * Envelope::rate;
            _nextStageAt = _currSample + samplesLeft;
            calcStageMultiplier(_level, nextLevel, samplesLeft);
        }
    }

protected:
    EnvelopeStage getNextStage () const
    {
        return _next[_currStage];
    }

    /* 
     * Calculates a multiplier used to change the output level from `start' to
     * `end' over `_nextStageAt' number of samples. Since ears logarithmically,
     * i.e. exponential changes sound linear to the ear, this calculates the
     * exponential curve between two points. Rather than call `exp' this is
     * a faster optimization.
     */
    void calcStageMultiplier (double start, double end, size_t numSamples)
    {
        _multiplier = 1.0 + (log(end) - log(start)) / numSamples;
    }

    void enterStage (EnvelopeStage stage)
    {
        _currStage = stage;

        _currSample = 0;
        if (_currStage == STAGE_SUSTAIN)
            _nextStageAt = 0;
        else
            _nextStageAt = _values[_currStage] * Envelope::rate;

        switch (_currStage) {
            case STAGE_ATTACK:
                _level = _minLevel;
                calcStageMultiplier(_level, 1.0, _nextStageAt);
                break;

            case STAGE_DECAY:
                _level = 1.0;
                calcStageMultiplier(_level,
                        fmax(_values[STAGE_SUSTAIN], _minLevel),
                        _nextStageAt);
                break;

            case STAGE_SUSTAIN:
                _level = _values[STAGE_SUSTAIN];
                _multiplier = 1.0;
                break;

            case STAGE_RELEASE:
                /* 
                 * Because this stage can be entered by any stage by releasing
                 * the key, let it `decay' from the current output level.
                 */
                calcStageMultiplier(_level, _minLevel, _nextStageAt);
                break;

            default:
                break;
        }
    }

private:
    const double _minLevel;
    double _level;
    double _multiplier;

    EnvelopeStage _currStage;

    double _values[NUM_STAGES];
    EnvelopeStage _next[NUM_STAGES];

    size_t _currSample;
    size_t _nextStageAt;
};
unsigned long Envelope::rate = 44100;

#include <unordered_map>
#include <cassert>
#include "Definitions.hpp"

class PolyNote {
public:
    /* PolyNotes start in the active state */
    PolyNote (const double frequency,
              const double velocity,
              double ADSR[4],
              const double cutoff,
              const double resonance,
              double filterADSR[4])
        : _isActive (false)
        , _velocity (0.0)
        , _filter (Filter(cutoff, resonance))
        , _env (Envelope(ADSR))
        , _filterEnv (Envelope(filterADSR))
    {
        noteOn(velocity);
        _filter.setMode(FILTER_LOWPASS);
        _oscillator.setMode(OSCILLATOR_MODE_SQUARE);
        _oscillator.setFreq(frequency);
        _oscillator.unmute();
    }

    /* Resets the envelope and note if already active */
    void noteOn (const double velocity)
    {
        _isActive = true;
        _velocity = velocity;
        _env.noteOn();
    }

    void noteOff ()
    {
        _env.noteOff();
    }

    bool isActive () const
    {
        return _isActive;
    }

    void setPitch (double value)
    {
        _oscillator.setPitch(value);
    }

    void setADSR (EnvelopeStage stage, double value)
    {
        _env.setValue(stage, value);
    }

    void setFilterCutoff (double value)
    {
        _filter.setCutoff(value);
    }

    void setFilterResonance (double value)
    {
        _filter.setResonance(value);
    }

    void setFilterADSR (EnvelopeStage stage, double value)
    {
        switch (stage) {
            case STAGE_ATTACK:
                printf("ATTACK: %lf\n", value);
                break;
            case STAGE_DECAY:
                printf("DECAY: %lf\n", value);
                break;
            case STAGE_SUSTAIN:
                printf("SUSTAIN: %lf\n", value);
                break;
            case STAGE_RELEASE:
                printf("RELEASE: %lf\n", value);
                break;
            default:
                printf("bad: %lf\n", value);
                break;
        }
        _filterEnv.setValue(stage, value);
    }

    double next ()
    {
        assert(_isActive);
        _isActive = _env.isActive();
        _filter.setCutoffMod(_filterEnv.next() * 0.8);
        return _filter.process(_oscillator.next() * _env.next() * _velocity);
    }

private:
    bool _isActive;
    double _velocity;
    Filter _filter;
    Envelope _env;
    Envelope _filterEnv;
    Oscillator _oscillator;
};

class Polyphonic {
public:
    Polyphonic (double attack, double decay, double sustain, double release)
    {
        _noteADSR[STAGE_ATTACK] = attack;
        _noteADSR[STAGE_DECAY] = decay;
        _noteADSR[STAGE_SUSTAIN] = sustain;
        _noteADSR[STAGE_RELEASE] = release;

        _filterCutoff = 0.99;
        _filterResonance = 0.0;
        _filterADSR[STAGE_ATTACK]  = 0.4;
        _filterADSR[STAGE_DECAY]   = 0.4;
        _filterADSR[STAGE_SUSTAIN] = 0.01;
        _filterADSR[STAGE_RELEASE] = 0.01;
    }

    void noteOn (const int note, const double velocity)
    {
        auto it = _notes.find(note);
        if (it != _notes.end()) {
            /* turn note back on if it already exists */
            it->second.noteOn(velocity);
        } else {
            /* otherwise just create it */
            double freq = 440.0 * pow(2.0, (note - 69.0) / 12.0);
            _notes.insert({note, PolyNote(freq, velocity, _noteADSR,
                        _filterCutoff, _filterResonance, _filterADSR)});
        }
    }

    void noteOff (const int note)
    {
        auto it = _notes.find(note);
        /* MIDI keyboard sometimes sends errant 'noteOff' events */
        if (it == _notes.end())
            return;
        it->second.noteOff();
    }

    bool noteActive (const int note)
    {
        auto it = _notes.find(note);
        if (it == _notes.end())
            return false;
        return it->second.isActive();
    }

    void setPitch (double value)
    {
        for (auto it = _notes.begin(); it != _notes.end(); it++)
            it->second.setPitch(value);
    }

    void setADSR (EnvelopeStage stage, double value)
    {
        _noteADSR[stage] = value;
        for (auto it = _notes.begin(); it != _notes.end(); it++)
            it->second.setADSR(stage, value);
    }

    void setFilterADSR (EnvelopeStage stage, double value)
    {
        _filterADSR[stage] = value;
        for (auto it = _notes.begin(); it != _notes.end(); it++)
            it->second.setFilterADSR(stage, value);
    }

    void setFilterCutoff (double value)
    {
        _filterCutoff = value;
        for (auto it = _notes.begin(); it != _notes.end(); it++)
            it->second.setFilterCutoff(value);
    }

    void setFilterResonance (double value)
    {
        _filterResonance = value;
        for (auto it = _notes.begin(); it != _notes.end(); it++)
            it->second.setFilterResonance(value);
    }

    double next ()
    {
        double out = 0.0;
        /* This weird construction allows removing objects while iterating */
        for (auto it = _notes.begin(); it != _notes.end(); ) {
            if (!it->second.isActive()) {
                if (DEBUG)
                    printf("Removing note %2x\n", it->first);
                it = _notes.erase(it);
            } else {
                out += it->second.next();
                it++;
            }
        }
        return out;
    }

private:
    double _noteADSR[4];
    double _filterADSR[4];
    double _filterResonance;
    double _filterCutoff;
    std::unordered_map<int, PolyNote> _notes;
};

int
main (int argc, char **argv)
{
    signal(SIGINT, sigint);

    if (argc < 6) {
        fprintf(stderr, "./synth <MidiDevice> <a> <d> <s> <r>\n");
        exit(1);
    }

    const char *midiDevice = argv[1];
    double attack = atof(argv[2]);
    double decay = atof(argv[3]);
    double sustain = atof(argv[4]);
    double release = atof(argv[5]);

    AudioDevice audio;

    size_t rate = audio.getRate();
    size_t samples_len = audio.getPeriodSamples();
    int16_t *samples = new int16_t[samples_len];

    Oscillator::setRate(rate);
    Envelope::setRate(rate);
    MidiController midi(midiDevice);
    Polyphonic polyphonic(attack, decay, sustain, release);

    while (progRunning) {
        for (unsigned i = 0; i < samples_len; i += 2) {
            MidiEvent e = midi.nextEvent();
            switch (e.type) {
                case MIDI_NOTEON:
                    polyphonic.noteOn(e.note, e.velocity);
                    break;
                case MIDI_NOTEOFF:
                    polyphonic.noteOff(e.note);
                    break;
                case MIDI_PITCHBEND:
                    polyphonic.setPitch(e.pitch);
                    break;
                case MIDI_CONTROL:
                    if (e.note <= 4)
                        /* minus 1 because control params start at 1 */
                        polyphonic.setADSR((EnvelopeStage)(e.note - 1), e.control);
                    else if (e.note == 5)
                        polyphonic.setFilterCutoff(e.control);
                    else if (e.note == 6)
                        polyphonic.setFilterResonance(e.control);
                    //else if (e.note <= 8)
                    //    /* minus 5 because stages are 1 through 4 */
                    //    polyphonic.setFilterADSR((EnvelopeStage)(e.note - 5), e.control);
                    break;
                default:
                    break;
            }
            samples[i] = samples[i + 1] = clip(polyphonic.next());
        }
        audio.play(samples, samples_len);
    }

    delete[] samples;
    return 0;
}
