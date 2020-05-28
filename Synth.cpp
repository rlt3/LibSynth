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
midiProcess (snd_seq_t *handle, Oscillator &O)
{
    /* 
     * try to consume `num_events' of input from the input buffer. This prevents
     * overflows of the internal event queue of the sequencer.
     */
    const int num_events = 10;
    snd_seq_event_t *ev = NULL;
    int r;

    for (int i = 0; i < num_events; i++) {
        r = snd_seq_event_input(handle, &ev);
        if (r == -EAGAIN) {
            break;
        }
        else if (r < 0) {
            fprintf(stderr, "midiRead: %s\n", snd_strerror(r));
            exit(1);
        }

        switch (ev->type) {
            case SND_SEQ_EVENT_PITCHBEND:
                printf("[%d] Pitchbend:  val(%2x) = %lf\n",
                        ev->time.tick,
                        ev->data.control.value,
                        (double) ev->data.control.value / 8192.0);
                O.setPitch((double) ev->data.control.value / 8192.0);
                break;

            case SND_SEQ_EVENT_CONTROLLER:
                printf("[%d] Control:  %2x val(%2x)\n", ev->time.tick,
                                                        ev->data.control.param,
                                                        ev->data.control.value);
                break;

            case SND_SEQ_EVENT_NOTEON:
                printf("[%d] Note on: %2x vel(%2x)\n", ev->time.tick,
                                                       ev->data.note.note,
                                                       ev->data.note.velocity);
                break;        

            case SND_SEQ_EVENT_NOTEOFF:
                printf("[%d] Note off: %2x vel(%2x)\n", ev->time.tick,
                                                       ev->data.note.note,
                                                       ev->data.note.velocity);
                break;        
        }
    }
}

void
midiClose (snd_seq_t *handle)
{
    snd_seq_close(handle);
}

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

    snd_seq_t *midi;
    int midi_port;

    midiOpen(&midi, &midi_port);

    printf("event size: %lu\n", sizeof(snd_seq_event_t));
    printf("input buffer size: %lu\n", snd_seq_get_input_buffer_size(midi));
    snd_seq_set_input_buffer_size(midi, sizeof(snd_seq_event_t) * 5000);
    printf("input buffer size: %lu\n", snd_seq_get_input_buffer_size(midi));

    while (1) {
        midiProcess(midi, oscillator);
        for (unsigned i = 0; i < samples_len; ++i) {
            samples[i] = clip(oscillator.next());
        }
        PCM.play(samples, samples_len);
    }

    midiClose(midi);
    free(samples);
    return 0;
}
