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
    return 32767.0 * x;
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

class Envelope {
public:
    /* Sample rate for all envelopes */
    static unsigned long rate;

    typedef enum _EnvelopeStage {
        STAGE_OFF = 0,
        STAGE_ATTACK,
        STAGE_DECAY,
        STAGE_SUSTAIN,
        STAGE_RELEASE,
        NUM_STAGES,
    } EnvelopeStage;

    Envelope (double attack, double decay, double sustain, double release)
        : _minLevel (0.0001)
        , _level (_minLevel)
        , _multiplier (1.0) 
        , _stage (STAGE_OFF)
        , _currSample (0)
        , _nextStageAt (0)
    {
        /* determines when the next stage occurs */
        _values[STAGE_OFF]     = 0.0;
        _values[STAGE_ATTACK]  = attack;
        _values[STAGE_DECAY]   = decay;
        _values[STAGE_SUSTAIN] = sustain;
        _values[STAGE_RELEASE] = release;

        /* simple state transition table */
        _next[STAGE_OFF]     = STAGE_OFF;
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
        if (_stage == STAGE_OFF)
            return false;
        if (_stage == STAGE_RELEASE && _level <= _minLevel)
            return false;
        return true;
    }

    double next ()
    {
        if (!(_stage == STAGE_OFF || _stage == STAGE_SUSTAIN)) {
            if (_currSample == _nextStageAt)
                enterStage(getNextStage());
            _level *= _multiplier;
            _currSample++;
        }
        return _level;
    }

    /* Set the sample rate for all envelopes created */
    static void setRate (unsigned long rate)
    {
        Envelope::rate = rate;
    }

protected:
    EnvelopeStage getNextStage () const
    {
        return _next[_stage];
    }

    /* 
     * Calculates a multiplier used to change the output level from `start' to
     * `end' over `_nextStageAt' number of samples. Since ears logarithmically,
     * i.e. exponential changes sound linear to the ear, this calculates the
     * exponential curve between two points. Rather than call `exp' this is
     * a faster optimization.
     */
    void calcStageMultiplier (double start, double end)
    {
        _multiplier = 1.0 + (log(end) - log(start)) / _nextStageAt;
    }

    void enterStage (EnvelopeStage stage)
    {
        _stage = stage;

        _currSample = 0;
        if (_stage == STAGE_OFF || _stage == STAGE_SUSTAIN)
            _nextStageAt = 0;
        else
            _nextStageAt = _values[_stage] * Envelope::rate;

        switch (_stage) {
            case STAGE_OFF:
                _level = 0.0;
                _multiplier = 1.0;
                break;

            case STAGE_ATTACK:
                _level = _minLevel;
                calcStageMultiplier(_level, 1.0);
                break;

            case STAGE_DECAY:
                _level = 1.0;
                calcStageMultiplier(_level, fmax(_values[STAGE_SUSTAIN], _minLevel));
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
                calcStageMultiplier(_level, _minLevel);
                break;

            default:
                break;
        }
    }

private:
    const double _minLevel;
    double _level;
    double _multiplier;

    EnvelopeStage _stage;

    double _values[NUM_STAGES];
    EnvelopeStage _next[NUM_STAGES];

    unsigned long _currSample;
    unsigned long _nextStageAt;
};
unsigned long Envelope::rate = 44100;

#include <unordered_map>
#include <cassert>
#include "Definitions.hpp"

class PolyNote {
public:
    /* PolyNotes start in the active state */
    PolyNote (const double frequency, const double velocity)
        : _isActive (false)
        , _velocity (0.0)
        , _env (Envelope(0.01, 0.5, 0.1, 1.0))
    {
        noteOn(velocity);
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

    double next ()
    {
        assert(_isActive);
        _isActive = _env.isActive();
        return  _oscillator.next() * _env.next() * _velocity;
    }

private:
    bool _isActive;
    double _velocity;
    Envelope _env;
    Oscillator _oscillator;
};

class Polyphonic {
public:
    Polyphonic ()
    { }

    void noteOn (const int note, const double velocity)
    {
        auto it = _notes.find(note);
        if (it != _notes.end()) {
            /* turn note back on if it already exists */
            it->second.noteOn(velocity);
        } else {
            /* otherwise just create it */
            double freq = 440.0 * pow(2.0, (note - 69.0) / 12.0);
            _notes.insert({note, PolyNote(freq, velocity)});
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
    std::unordered_map<int, PolyNote> _notes;
};

int
main (int argc, char **argv)
{
    signal(SIGINT, sigint);

    if (argc < 2) {
        fprintf(stderr, "./synth <MidiDevice>\n");
        exit(1);
    }

    const char *midiDevice = argv[1];

    //if (argc < 5) {
    //    fprintf(stderr,
    //            "./synth <a> <d> <s> <r>\n"); 
    //    exit(1);
    //}
    //double attack = atof(argv[1]);
    //double decay = atof(argv[2]);
    //double sustain = atof(argv[3]);
    //double release = atof(argv[4]);

    AudioDevice audio;

    size_t rate = audio.getRate();
    size_t period_size = audio.getPeriodSize();
    size_t samples_len = period_size;
    size_t samples_bytes = samples_len * sizeof(double);
    double *samples = (double*) malloc(samples_bytes);

    Oscillator LFO;
    LFO.setMode(OSCILLATOR_MODE_TRIANGLE);
    LFO.useNaive(true);
    LFO.setFreq(40.0);
    double LFOFilter = 0.1;

    Oscillator::setRate(rate);
    Envelope::setRate(rate);
    MidiController midi(midiDevice);
    Polyphonic polyphonic;

    while (progRunning) {
        for (unsigned i = 0; i < samples_len; ++i) {
            MidiEvent e = midi.nextEvent();
            switch (e.type) {
                case MIDI_NOTEON:
                    polyphonic.noteOn(e.note, e.velocity);
                    break;
                case MIDI_NOTEOFF:
                    polyphonic.noteOff(e.note);
                    break;
                //case MIDI_PITCHBEND:
                //    polyphonic.setPitch(e.pitch);
                //    break;
                case MIDI_CONTROL:
                    if (e.note == 1) {
                        LFOFilter = e.control;
                        if (DEBUG)
                            printf("LFOFilter = %lf\n", LFOFilter);
                    }
                    break;
                default:
                    break;
            }
            samples[i] = clip(polyphonic.next());
        }
        audio.play(samples, samples_len);
    }

    free(samples);
    return 0;
}
