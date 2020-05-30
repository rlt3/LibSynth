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
    typedef enum _EnvelopeStage {
        STAGE_OFF = 0,
        STAGE_ATTACK,
        STAGE_DECAY,
        STAGE_SUSTAIN,
        STAGE_RELEASE,
        NUM_STAGES,
    } EnvelopeStage;

    Envelope (double attack, double decay, double sustain, double release)
        : _minOut (0.0001)
        , _out (_minOut)
        , _multiplier (1.0) 
        , _stage (STAGE_OFF)
        , _currSample (0)
        , _nextStageAt (0)
    {
        _values[STAGE_OFF]     = 0.0;
        _values[STAGE_ATTACK]  = attack;
        _values[STAGE_DECAY]   = decay;
        _values[STAGE_SUSTAIN] = sustain;
        _values[STAGE_RELEASE] = release;

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

    double next ()
    {
        if (!(_stage == STAGE_OFF || _stage == STAGE_SUSTAIN)) {
            if (_currSample == _nextStageAt)
                enterStage(getNextStage());
            _out *= _multiplier;
            _currSample++;
        }
        return _out;
    }

protected:
    EnvelopeStage getNextStage ()
    {
        return _next[_stage];
    }

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
            _nextStageAt = _values[_stage] * 44100;

        switch (_stage) {
            case STAGE_OFF:
                _out = 0.0;
                _multiplier = 1.0;
                break;

            case STAGE_ATTACK:
                _out = _minOut;
                calcStageMultiplier(_out, 1.0);
                break;

            case STAGE_DECAY:
                _out = 1.0;
                calcStageMultiplier(_out, fmax(_values[STAGE_SUSTAIN], _minOut));
                break;

            case STAGE_SUSTAIN:
                _out = _values[STAGE_SUSTAIN];
                _multiplier = 1.0;
                break;

            case STAGE_RELEASE:
                /* 
                 * Because this stage can be entered by any stage by releasing
                 * the key, let it `decay' from the current output level.
                 */
                calcStageMultiplier(_out, _minOut);
                break;

            default:
                break;
        }
    }

private:
    const double _minOut;
    double _out;
    double _multiplier;

    EnvelopeStage _stage;

    double _values[NUM_STAGES];
    EnvelopeStage _next[NUM_STAGES];

    unsigned long _currSample;
    unsigned long _nextStageAt;
};

int
main (int argc, char **argv)
{
    signal(SIGINT, sigint);

    if (argc < 5) {
        fprintf(stderr,
                "./synth <a> <d> <s> <r>\n"); 
        exit(1);
    }
    double attack = atof(argv[1]);
    double decay = atof(argv[2]);
    double sustain = atof(argv[3]);
    double release = atof(argv[4]);

    AudioDevice PCM;
    Oscillator oscillator;
    MidiController midi;
    Envelope env(attack, decay, sustain, release);

    size_t period_size = PCM.getPeriodSize();
    size_t samples_len = period_size;
    size_t samples_bytes = samples_len * sizeof(double);
    double *samples = (double*) malloc(samples_bytes);

    oscillator.setRate(PCM.getRate());
    oscillator.setFreq(440.0);
    oscillator.unmute();

    int lastNote = -1;

    while (progRunning) {
        for (unsigned i = 0; i < samples_len; ++i) {
            midi.process();
            if (midi.velocity() > 0) {
                oscillator.setFreq(midi.frequency());
                oscillator.setPitch(midi.pitch());
            }
            if (midi.note() > 0) {
                lastNote = midi.note();
                env.noteOn();
            }
            else if (lastNote > 0 && !midi.noteOn(lastNote)) {
                env.noteOff();
                lastNote = -1;
            }
            samples[i] = clip(oscillator.next()  * env.next());
            //samples[i] = clip(oscillator.next()  * env.next() * midi.velocity());
        }
        PCM.play(samples, samples_len);
    }

    free(samples);
    return 0;
}
