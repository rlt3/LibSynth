#include <cstdlib>
#include <cstdio>
#include <csignal>
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

int
main ()
{
    signal(SIGINT, sigint);

    AudioDevice PCM;
    Oscillator oscillator;
    MidiController midi;

    size_t period_size = PCM.getPeriodSize();
    size_t samples_len = period_size;
    size_t samples_bytes = samples_len * sizeof(double);
    double *samples = (double*) malloc(samples_bytes);

    oscillator.setRate(PCM.getRate());
    oscillator.setFreq(440.0);
    oscillator.mute();

    while (progRunning) {
        for (unsigned i = 0; i < samples_len; ++i) {
            midi.process();
            if (midi.velocity() > 0) {
                oscillator.unmute();
                oscillator.setFreq(midi.frequency());
                oscillator.setPitch(midi.pitch());
            } else {
                oscillator.mute();
            }
            samples[i] = clip(oscillator.next() * midi.velocity());
        }
        PCM.play(samples, samples_len);
    }

    free(samples);
    return 0;
}
