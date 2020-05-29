#include <cstdlib>
#include <cstdio>
#include <csignal>
#include "AudioDevice.hpp"
#include "Oscillator.hpp"
#include "MidiController.hpp"

static int16_t
clip (double x)
{
    if (x > 1.0)
        x = 1.0;
    else if (x < -1.0)
        x = -1.0;
    return 32767.0 * x;
}

/*
 * TODO:
 * Need to create MidiController which spawns off a thread collect events from
 * the sequencer. These events are sent to the MidiController through a method
 * and put into a queue. The events are 'timestamped' with a long unsigned
 * integer.
 *
 * Collecting events will work much in the same way as now: when there are
 * events ready to collect then collect them all and sum them up into one
 * single MidiEvent. We will use a similar construction to the MidiQueue seen
 * in the IPlug2 interface and from Martin Finke:
        for (int i = 0; i < nFrames; ++i) {
            MidiEvent &e = MidiController.nextEvent();
            * sample production based on event *
        }
        MidiController.flush(nFrames);
    
    The `flush' method controls the internal `timestamp' of the MidiController
    and can probably be better named -- it does not 'flush' or 'drain' or get
    rid of any events in the queue at all. In actuality, it just subtracts the
    timestamp of every remaining event in the queue by `nFrames'
    
    The `nextEvent' method will return a default `blank' MidiEvent (all zeros 
    or default values) when the timestamp of the next event in the queue is
    greater than the MidiController's current timestamp (see
    09_SpaceBass/MIDIReciever.cpp).
 */

static bool progRunning = true;

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
    //size_t samples_len = period_size * 8;
    size_t samples_len = period_size;
    size_t samples_bytes = samples_len * sizeof(double);
    double *samples = (double*) malloc(samples_bytes);

    oscillator.setRate(PCM.getRate());
    oscillator.setFreq(440.0);
    //oscillator.mute();

    while (progRunning) {
        for (unsigned i = 0; i < samples_len; ++i) {
            //midi.process();
            //if (midi.velocity > 0) {
            //    oscillator.unmute();
            //    oscillator.setFreq(midi.frequency);
            //    oscillator.setPitch(midi.pitch);
            //} else {
            //    oscillator.mute();
            //}
            samples[i] = clip(oscillator.next());
        }
        PCM.play(samples, samples_len);
    }

    free(samples);
    return 0;
}
