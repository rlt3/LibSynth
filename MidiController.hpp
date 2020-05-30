#ifndef MIDICONTROLLER_HPP
#define MIDICONTROLLER_HPP

#include <map>
#include <queue>
#include <pthread.h>

typedef enum _MidiEventType {
    MIDI_PITCHBEND,
    MIDI_NOTEON,
    MIDI_NOTEOFF,
    MIDI_CONTROL,
    MIDI_UNHANDLED,
    MIDI_EMPTY,
} MidiEventType;

struct MidiEvent {
    MidiEventType type;
    int note;
    double control;
    double velocity;
    double pitch;

    MidiEvent (MidiEventType t, int n, double c, double v, double p)
        : type (t)
        , note (n)
        , control (c)
        , velocity (v)
        , pitch (p)
    { }

    MidiEvent (MidiEventType t)
        : type (t)
        , note (0)
        , control (0.0)
        , velocity (0.0)
        , pitch (0.0)
    { }

    MidiEvent ()
        : type (MIDI_UNHANDLED)
        , note (0)
        , control (0.0)
        , velocity (0.0)
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

    int note () const;
    bool noteOn (int note) const;

    /* Process the next event to update the current state. */
    void process ();

    /* Lock the queue and insert the event. */
    void input (MidiEvent event);

    /*
     * Returns an Event from the queue if available. Otherwise, returns an
     * event with type MIDI_EMPTY indicating queue is empty.
     */
    MidiEvent nextEvent ();

protected:

private:
    void *_sequencer; 

    double _frequency;
    double _velocity;
    double _pitch;
    int    _note;

    std::queue<MidiEvent> _queue;
    pthread_t _eventThread;
    pthread_mutex_t _eventQueueLock;
    bool _eventThreadWorking;

    std::map<int, bool> _notes;
};

#endif
