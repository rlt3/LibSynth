#ifndef MIDICONTROLLER_HPP
#define MIDICONTROLLER_HPP

#include <queue>
#include <utility>
#include <pthread.h>

typedef enum _MidiEventType {
    MIDI_PITCHBEND,
    MIDI_NOTEON,
    MIDI_NOTEOFF,
    MIDI_UNHANDLED,
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

    /* 
     * Sync the remaining events in the queue by subtracting the current number
     * of frames processed from them.
     */
    void sync (int framesProcessed);

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
    double _velocity;
    double _pitch;

    unsigned long _timestamp;

    std::queue<std::pair<int, MidiEvent>> queue;
    pthread_t _eventThread;
    pthread_mutex_t _eventQueueLock;
    bool _eventThreadWorking;
};

#endif
