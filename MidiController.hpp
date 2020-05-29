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
    unsigned long timestamp;

    MidiEvent (MidiEventType t, int n, int v, double p, unsigned long s)
        : type (t)
        , note (n)
        , velocity (v)
        , pitch (p)
        , timestamp (s)
    { }

    MidiEvent (MidiEventType t)
        : type (t)
        , note (0)
        , velocity (0)
        , pitch (0.0)
        , timestamp (0)
    { }

    MidiEvent ()
        : type (MIDI_UNHANDLED)
        , note (0)
        , velocity (0)
        , pitch (0.0)
        , timestamp (0)
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

    /* 
     * Sync the timestamps. Note: not used right now, but will be probably used
     * to prevent overflow of unsigned long timestamp value in event thread.
     */
    void sync (unsigned long timestamp);

    /* Lock the queue and insert the event. */
    void input (MidiEvent event);

protected:
    /* 
     * Lock the queue and pop the next event off the queue if the internal
     * timestamp is greater than the next event. Else, return a 'blank' event.
     */
    MidiEvent nextEvent ();

private:
    void *_sequencer; 

    double _frequency;
    int _velocity;
    double _pitch;

    unsigned long _timestamp;

    std::queue<MidiEvent> _queue;
    pthread_t _eventThread;
    pthread_mutex_t _eventQueueLock;
    bool _eventThreadWorking;
};

#endif
