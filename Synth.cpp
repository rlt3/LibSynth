#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <cmath>
#include "Definitions.hpp"
#include "AudioDevice.hpp"
#include "Oscillator.hpp"
#include "MidiController.hpp"
#include "Filter.hpp"
#include "Envelope.hpp"
#include "Polyphonic.hpp"

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

class Synth {
public:
    Synth ()
    {
        init(NULL);
    }

    Synth (const char *midiDevice)
    {
        init(midiDevice);
    }

    Synth (const std::string midiDevice)
    {
        init(midiDevice.c_str());
    }

    ~Synth ()
    {
        _running = false;
        pthread_join(_thread, NULL);
        delete _audio;
        delete _midi;
        delete _polyphonic;
        delete[] _samples;
    }

    void setVolume (const double value)
    {
        _volume = clamp(value, 0.0, 1.5);
    }

    void setWaveform (const OscillatorWave wave)
    {
        _polyphonic->setWaveForm(wave);
    }

    void setAttack (const double value) const
    {
        _polyphonic->setADSR(STAGE_ATTACK, clamp(value, 0.01, 1.0));
    }

    void setDecay (const double value) const
    {
        _polyphonic->setADSR(STAGE_DECAY, clamp(value, 0.01, 1.0));
    }

    void setSustain (const double value) const
    {
        _polyphonic->setADSR(STAGE_SUSTAIN, clamp(value, 0.01, 1.0));
    }

    void setRelease (const double value) const
    {
        _polyphonic->setADSR(STAGE_RELEASE, clamp(value, 0.01, 1.0));
    }

    void setCutoff (const double value) const
    {
        _polyphonic->setFilterCutoff(clamp(value, 0.0, 0.99));
    }

    void setResonance (const double value) const
    {
        _polyphonic->setFilterResonance(clamp(value, 0.0, 0.99));
    }

    void setFilterAttack (const double value) const
    {
        _polyphonic->setFilterADSR(STAGE_ATTACK, clamp(value, 0.01, 1.0));
    }

    void setFilterDecayFilter (const double value) const
    {
        _polyphonic->setFilterADSR(STAGE_DECAY, clamp(value, 0.01, 1.0));
    }

    void setFilterSustaFilterin (const double value) const
    {
        _polyphonic->setFilterADSR(STAGE_SUSTAIN, clamp(value, 0.01, 1.0));
    }

    void setFilterReleaFilterse (const double value) const
    {
        _polyphonic->setFilterADSR(STAGE_RELEASE, clamp(value, 0.01, 1.0));
    }

    void noteOn (const int note, const double velocity) const
    {
        _midi->input(MidiEvent(MIDI_NOTEON, note, 0.0, clamp(velocity, 0.0, 1.0), 0.0));
    }

    void noteOff (const int note) const
    {
        _midi->input(MidiEvent(MIDI_NOTEOFF, note, 0.0, 0.0, 0.0));
    }

protected:
    void init (const char *midiDevice)
    {
        _volume = 1.0;
        _audio = new AudioDevice();

        size_t rate = _audio->getRate();
        _samplesLen = _audio->getPeriodSamples();
        _samples = new int16_t[_samplesLen];

        Oscillator::setRate(rate);
        Envelope::setRate(rate);
        _midi = new MidiController(midiDevice);

        /* 
         * A simple default. Short attack, medium decay and sustain, long
         * release. The filter's ADSR should produce a 'tingy' sound with no
         * resonance and a high cutoff.
         */
        _polyphonic = new Polyphonic(
                            0.01, 0.5, 0.5, 1.0,
                            0.2, 0.2, 1.0, 1.0,
                            0.99, 0.0);
        _polyphonic->setWaveForm(OSCILLATOR_WAVE_SQUARE);

        _running = true;
        CHK(pthread_create(&_thread, NULL, Synth::audio_thread, this),
                "Could not create audio thread");
    }

    static void* audio_thread (void *data)
    {
        Synth *synth = (Synth*) data;
        Polyphonic *polyphonic = synth->_polyphonic;
        MidiController *midi = synth->_midi;
        AudioDevice *audio = synth->_audio;
        int16_t *samples = synth->_samples;
        size_t samplesLen = synth->_samplesLen;

        while (synth->_running) {
            for (unsigned i = 0; i < samplesLen; i += 2) {
                MidiEvent e = midi->nextEvent();
                switch (e.type) {
                    case MIDI_NOTEON:
                        polyphonic->noteOn(e.note, e.velocity);
                        break;
                    case MIDI_NOTEOFF:
                        polyphonic->noteOff(e.note);
                        break;
                    case MIDI_PITCHBEND:
                        polyphonic->setPitch(e.pitch);
                        break;
                    case MIDI_CONTROL:
                        if (e.note <= 4)
                            /* minus 1 because control params start at 1 */
                            polyphonic->setADSR((EnvelopeStage)(e.note - 1), e.control);
                        else if (e.note == 5)
                            polyphonic->setFilterCutoff(e.control);
                        else if (e.note == 6)
                            polyphonic->setFilterResonance(e.control);
                        else if (e.note <= 10)
                            /* minus 6 because stages are 1 through 4 */
                            polyphonic->setFilterADSR((EnvelopeStage)(e.note - 6), e.control);
                        break;
                    default:
                        break;
                }
                samples[i] = samples[i + 1] = clip(synth->_volume * polyphonic->next());
            }
            audio->play(samples, samplesLen);
        }

        return NULL;
    }

private:
    AudioDevice    *_audio;
    MidiController *_midi;
    Polyphonic     *_polyphonic;
    int16_t        *_samples;
    size_t          _samplesLen;
    double          _volume;

    bool _running;
    pthread_t _thread;
};

#include <unistd.h>

int
main (int argc, char **argv)
{
    signal(SIGINT, sigint);

    char *midiDevice = NULL;

    if (argc >= 2)
        midiDevice = argv[1];

    Synth synth(midiDevice);

    /* how hard the note is played (how loud it will be) in range [0.0, 1.0] */
    const double velocity = 1.0;
    /* a low note */
    const int note = 32;

    bool noteon = true;
    while (progRunning) {
        /* play a note every second, turning it on and off every half second */
        if (noteon)
            synth.noteOn(note, velocity);
        else
            synth.noteOff(note);
        noteon = !noteon;
        usleep(500000);
    }

    return 0;
}
