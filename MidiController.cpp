#include <cmath>
#include <alsa/asoundlib.h>
#include "MidiController.hpp"
#include "Definitions.hpp"

static MidiEvent
_midi_event_process (snd_seq_event_t *ev)
{
    MidiEventType type = MIDI_UNHANDLED;
    int note = 0;
    double velocity = 0;
    double pitch = 0.0;

    switch (ev->type) {
        case SND_SEQ_EVENT_PITCHBEND:
        {
            if (DEBUG)
                printf("[%u] Pitchbend:  val(%2x)\n", ev->time.tick,
                                                      ev->data.control.value);
            type = MIDI_PITCHBEND;
            pitch = (double) ev->data.control.value / 8192.0;
            break;
        }

        case SND_SEQ_EVENT_CONTROLLER:
        {
            if (DEBUG)
               printf("[%u] Control:  %2x val(%2x)\n", ev->time.tick,
                                                       ev->data.control.param,
                                                       ev->data.control.value);
            break;
        }

        case SND_SEQ_EVENT_NOTEON:
        {
            if (DEBUG)
               printf("[%u] Note on: %2x vel(%2x)\n", ev->time.tick,
                                                      ev->data.note.note,
                                                      ev->data.note.velocity);
            if (ev->data.note.velocity > 0) {
                type = MIDI_NOTEON;
                note = ev->data.note.note;
                velocity = (double)ev->data.note.velocity / 127.0;
            }
            break;
        }

        case SND_SEQ_EVENT_NOTEOFF:
        {
           if (DEBUG)
               printf("[%u] Note off: %2x vel(%2x)\n", ev->time.tick,
                                                       ev->data.note.note,
                                                       ev->data.note.velocity);
           type = MIDI_NOTEOFF;
           note = ev->data.note.note;
           break;
        }
    }

    return MidiEvent(type, note, velocity, pitch);
}

struct MidiThreadData {
    bool *collecting_events;
    snd_seq_t *sequencer;
    MidiController *controller;
};

static void*
_midi_event_thread (void *data)
{
    MidiThreadData *threadData = (MidiThreadData*) data;
    bool *collecting_events = threadData->collecting_events;
    snd_seq_t *sequencer = threadData->sequencer;
    MidiController *controller = threadData->controller;
    delete threadData;

    bool set_pending;
    int events_pending;
    snd_seq_event_t *seq_event = NULL;
    int r;

    while (*collecting_events) {
        set_pending = false;
        events_pending = 0;
        seq_event = NULL;
        do {
            r = snd_seq_event_input(sequencer, &seq_event);
            if (r == -EAGAIN)
                break;
            if (r < 0) {
               fprintf(stderr, "_midi_event_thread: %s\n", snd_strerror(r));
               exit(1);
            }
            /*
             * Warning: `snd_seq_event_input_pending` seems to only 'work'
             * after having called `snd_seq_event_input'. Attempting to call it
             * before and then 'loop-to' the number of pending events does not
             * work for whatever reason.
             */
            if (!set_pending) {
               set_pending = true;
               events_pending = snd_seq_event_input_pending(sequencer, 0);
            }

            controller->input(_midi_event_process(seq_event));
            events_pending--;
        } while (events_pending > 0);
    }

    return NULL;
}

MidiController::MidiController ()
    : _sequencer (NULL)
    , _frequency (-1.0)
    , _velocity (0.0)
    , _pitch (0.0)
{
    snd_seq_t *handle = NULL;
    int seq_port;

    /* Setup the ALSA MIDI Sequencer */
    CHK(snd_seq_open(&handle, "default", SND_SEQ_OPEN_INPUT, 0),
            "Could not open sequencer");

    CHK(snd_seq_set_client_name(handle, "Midi Listener"),
            "Could not set client name");

    CHK(seq_port = snd_seq_create_simple_port(handle, "listen:in",
                    SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                    SND_SEQ_PORT_TYPE_APPLICATION),
            "Could not open port");

    CHK(snd_seq_nonblock(handle, 1), "Could not set non-blocking");

    /* Create the struct to pass data to the event thread */
    MidiThreadData *data = new MidiThreadData();
    data->sequencer = handle;
    data->controller = this;
    data->collecting_events = &_eventThreadWorking;

    /* Finally start the thread */
    _sequencer = handle;
    _eventThreadWorking = true;
    pthread_mutex_init(&_eventQueueLock, NULL);
    CHK(pthread_create(&_eventThread, NULL, _midi_event_thread, data),
            "Could not create event thread");
}

MidiController::~MidiController ()
{
    _eventThreadWorking = false;
    pthread_join(_eventThread, NULL);
    snd_seq_close((snd_seq_t*) _sequencer);
}

double
MidiController::frequency () const
{
    return _frequency;
}

double
MidiController::velocity () const
{
    return _velocity;
}

double
MidiController::pitch () const
{
    return _pitch;
}

int
MidiController::note () const
{
    return _note;
}

bool
MidiController::noteOn (int note) const
{
    auto iter = _notes.find(note);
    if (iter == _notes.end())
        return false;
    return iter->second;
}

static inline double
noteToFrequency (int note)
{
    return 440.0 * pow(2.0, (note - 69.0) / 12.0);
}

void
MidiController::process ()
{
    _note = -1;
    MidiEvent e = nextEvent();
    if (e.type == MIDI_EMPTY)
        return;

    switch (e.type) {
        case MIDI_NOTEON:
            if (e.type == MIDI_NOTEON && e.velocity) {
                _note = e.note;
                _frequency = noteToFrequency(_note);
                _velocity = e.velocity;
                _notes[_note] = true;
            }
            break;

        case MIDI_NOTEOFF:
            _notes[e.note] = false;
            _frequency = -1.0;
            _velocity = 0.0;
            break;

        case MIDI_PITCHBEND:
            _pitch = e.pitch;
            break;

        default:
            break;
    }
}

void
MidiController::input (MidiEvent event)
{
    pthread_mutex_lock(&_eventQueueLock);
    _queue.push(event);
    pthread_mutex_unlock(&_eventQueueLock);
}

/*
 * Returns an Event from the queue if available. Otherwise, returns an event
 * with type MIDI_EMPTY indicating queue is empty.
 */
MidiEvent
MidiController::nextEvent ()
{
    MidiEvent event(MIDI_EMPTY);
    pthread_mutex_lock(&_eventQueueLock);
    if (_queue.empty())
        goto unlock;
    event = _queue.front();
    _queue.pop();
unlock:
    pthread_mutex_unlock(&_eventQueueLock);
    return event;
}
