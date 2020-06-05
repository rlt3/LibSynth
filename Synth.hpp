#ifndef SYNTH_HPP
#define SYNTH_HPP

#include <pthread.h>
#include "AudioDevice.hpp"
#include "MidiController.hpp"
#include "Polyphonic.hpp"

class Synth {
public:
    /* Create a Synth with no MIDI device */
    Synth ();

    /* Create a Synth and attach it to the MIDI device by name */
    Synth (const char *midiDevice);
    Synth (const std::string midiDevice);

    ~Synth ();

    /*
     * Set the volume, a percentage of how loud the synth will be. Expects
     * values from 0.0 (muted) to 1.5. Default is 1.0.
     */
    void setVolume (const double value);

    /*
     * Set the waveform of the synth, e.g. sine, square, saw, etc.
     * See Oscillator.hpp
     */
    void setWaveform (const OscillatorWave wave);

    /* 
     * Set the ADSR envelope. Clamps values to range [0.0, 1.0]
     */
    void setAttack (const double value) const;
    void setDecay (const double value) const;
    void setSustain (const double value) const;
    void setRelease (const double value) const;

    /* 
     * Set the cutoff of the lowpass filter. Clamps values to range [0.0, 0.99]
     */
    void setCutoff (const double value) const;

    /* 
     * Set the resonance of the lowpass filter. Clamps values to range
     * [0.0, 0.99]
     */
    void setResonance (const double value) const;

    /*
     * Set the lowpass filter's ADSR envelope. Clamps values to range
     * [0.0, * 1.0]
     */
    void setFilterAttack (const double value) const;
    void setFilterDecay (const double value) const;
    void setFilterSustain (const double value) const;
    void setFilterRelease (const double value) const;

    /* 
     * Turn a single note on, playing it. Velocity is a value clamped to 
     * [0.0, 1.0] and reflects how loudly the note is played, i.e how hard it
     * was triggered on a keyboard or pad.
     */
    void noteOn (const int note, const double velocity) const;

    /* 
     * Turn a note off, releasing it. Note that turning a note off won't "stop"
     * any sound of that note immediately depending on the release value of
     * the ADSR envelope. This means calling noteOff won't immediate make that
     * note inactive.
     */
    void noteOff (const int note) const;

    /*
     * Returns true if the given note is currently playing, otherwise false.
     */
    bool noteActive (const int note) const;

protected:
    void init (const char *midiDevice);
    static void* audio_thread (void *data);

private:
    AudioDevice    *_audio;
    MidiController *_midi;
    Polyphonic     *_polyphonic;
    int16_t        *_samples;
    size_t          _samplesLen;
    double          _volume;

    bool _running;
    pthread_t _thread;
};

#endif
