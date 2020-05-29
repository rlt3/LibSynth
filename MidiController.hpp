#ifndef MIDICONTROLLER_HPP
#define MIDICONTROLLER_HPP

#include <queue>
#include <pthread.h>

typedef enum _MidiEventType {
    MIDI_PITCHBEND,
    MIDI_NOTEON,
    MIDI_NOTEOFF,
    MIDI_UNHANDLED,
    MIDI_EMPTY,
} MidiEventType;

struct MidiEvent {
    MidiEventType type;
    int note;
    int velocity;
    double pitch;

    MidiEvent (MidiEventType t, int n, int v, double p)
        : type (t)
        , note (n)
        , velocity (v)
        , pitch (p)
    { }

    MidiEvent (MidiEventType t)
        : type (t)
        , note (0)
        , velocity (0)
        , pitch (0.0)
    { }

    MidiEvent ()
        : type (MIDI_UNHANDLED)
        , note (0)
        , velocity (0)
        , pitch (0.0)
    { }
};

class MidiController {
public:
    MidiController ();
    ~MidiController ();

    double frequency () const;
    double velocity () const;
    double pitch () const;

    /* Process the next event to update the current state. */
    void process ();

    /* Lock the queue and insert the event. */
    void input (MidiEvent event);

protected:
    /*
     * Returns an Event from the queue if available. Otherwise, returns an
     * event with type MIDI_EMPTY indicating queue is empty.
     */
    MidiEvent nextEvent ();

private:
    void *_sequencer; 

    double _frequency;
    int _velocity;
    double _pitch;

    std::queue<MidiEvent> _queue;
    pthread_t _eventThread;
    pthread_mutex_t _eventQueueLock;
    bool _eventThreadWorking;
};

#endif
