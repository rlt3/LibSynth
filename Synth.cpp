#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "AudioDevice.hpp"
#include "Oscillator.hpp"

#include <alsa/asoundlib.h>

static int16_t
clip (double x)
{
    if (x > 1.0)
        x = 1.0;
    else if (x < -1.0)
        x = -1.0;
    return 32767.0 * x;
}

#define CHK(stmt, msg) if((stmt) < 0) {puts("ERROR: "#msg); exit(1);}

void
midiOpen (snd_seq_t **handle, int *port)
{
    CHK(snd_seq_open(handle, "default", SND_SEQ_OPEN_INPUT, 0),
        "Could not open sequencer");

    CHK(snd_seq_set_client_name(*handle, "Midi Listener"),
        "Could not set client name");

    CHK(*port = snd_seq_create_simple_port(*handle, "listen:in",
                    SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                    SND_SEQ_PORT_TYPE_APPLICATION),
        "Could not open port");

    CHK(snd_seq_nonblock(*handle, 1), "Could not set non-blocking");
}

void
midiClose (snd_seq_t *handle)
{
    snd_seq_close(handle);
}

#define KEYCOUNT 128
#define DEBUG false

inline double
noteNumberToFrequency (int noteNumber)
{
    return 440.0 * pow(2.0, (noteNumber - 69.0) / 12.0);
}

class MidiState {
public:
    int velocity;
    double frequency;
    double pitch;
    bool muted;
    int lastNote;

    MidiState (snd_seq_t *handle)
        : velocity (0)
        , frequency (-1.0)
        , pitch (0.0)
        , muted (true)
        , lastNote (-1)
        , handle(handle)
    {
    }

    void
    process ()
    {
        bool set_pending = false;
        int events_pending = 0;
        snd_seq_event_t *ev = NULL;
        int r;

        do {
            r = snd_seq_event_input(handle, &ev);
            if (r == -EAGAIN) {
                break;
            }
            else if (r < 0) {
                fprintf(stderr, "midiRead: %s\n", snd_strerror(r));
                exit(1);
            }
            /*
             * Warning: `snd_seq_event_input_pending` seems to only 'work' after
             * having called `snd_seq_event_input'. Attempting to call it before
             * and then 'loop-to' the number of pending events does not work for
             * whatever reason.
             */
            if (!set_pending) {
                set_pending = true;
                events_pending = snd_seq_event_input_pending(handle, 0);
            }

            switch (ev->type) {
                case SND_SEQ_EVENT_PITCHBEND:
                    if (DEBUG)
                        printf("[%u] Pitchbend:  val(%2x) = %lf\n",
                                ev->time.tick,
                                ev->data.control.value,
                                (double) ev->data.control.value / 8192.0);
                    pitch = (double) ev->data.control.value / 8192.0;
                    break;

                case SND_SEQ_EVENT_CONTROLLER:
                    if (DEBUG)
                        printf("[%u] Control:  %2x val(%2x)\n", ev->time.tick,
                                                                ev->data.control.param,
                                                                ev->data.control.value);
                    break;

                case SND_SEQ_EVENT_NOTEON:
                    if (DEBUG)
                        printf("[%u] Note on: %2x vel(%2x)\n", ev->time.tick,
                                                               ev->data.note.note,
                                                               ev->data.note.velocity);
                    if (ev->data.note.velocity > 0) {
                        frequency = noteNumberToFrequency(ev->data.note.note);
                        velocity = ev->data.note.velocity;
                    }
                    break;

                case SND_SEQ_EVENT_NOTEOFF:
                    if (DEBUG)
                        printf("[%u] Note off: %2x vel(%2x)\n", ev->time.tick,
                                                               ev->data.note.note,
                                                               ev->data.note.velocity);
                    lastNote = -1;
                    frequency = -1;
                    velocity = 0;
                    break;
            }

            events_pending--;
        } while (events_pending > 0);
    }

private:
    snd_seq_t *handle;
};


int
main ()
{
    AudioDevice PCM;
    Oscillator oscillator;

    size_t period_size = PCM.getPeriodSize();
    //size_t samples_len = period_size * 8;
    size_t samples_len = period_size;
    size_t samples_bytes = samples_len * sizeof(double);
    double *samples = (double*) malloc(samples_bytes);

    oscillator.setRate(PCM.getRate());
    oscillator.setFreq(440.0);
    oscillator.toggleMute();

    snd_seq_t *seq_handle;
    int seq_port;

    midiOpen(&seq_handle, &seq_port);

    MidiState midi(seq_handle);

    while (1) {
        for (unsigned i = 0; i < samples_len; ++i) {
            midi.process();
            if (midi.velocity > 0) {
                oscillator.toggleMute();
                oscillator.setFreq(midi.frequency);
                oscillator.setPitch(midi.pitch);
            } else {
                oscillator.toggleMute();
            }
            samples[i] = clip(oscillator.next() * (midi.velocity / 127.0));
        }
        PCM.play(samples, samples_len);
    }

    midiClose(seq_handle);
    free(samples);
    return 0;
}
