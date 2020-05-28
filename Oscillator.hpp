#ifndef OSCILLATOR_HPP
#define OSCILLATOR_HPP

/*
 * A PolyBLEP oscillator. Graciously borrowed from:
 * http://www.martin-finke.de/blog/articles/audio-plugins-018-polyblep-oscillator/
 */

enum OscillatorMode {
    OSCILLATOR_MODE_SINE,
    OSCILLATOR_MODE_SAW,
    OSCILLATOR_MODE_SQUARE,
    OSCILLATOR_MODE_TRIANGLE,
};

class Oscillator {
public:
    Oscillator ();

    /* get the next sample from the oscillator */
    double next ();

    void setMode  (enum OscillatorMode);
    void setFreq  (double);
    void setRate  (double);
    void setPitch (double);
    void toggleMute ();

protected:
    /* set the phase increment using the current values */
    void setIncrement ();

    /* approximates the sinc function with a triangle */
    double polyBlep (double);

    /* produce a naive (non-BLIT) wave using the current mode */
    double naiveWave ();

private:
    enum OscillatorMode _mode;

    /* sample rate */
    double _rate;
    /* frequency */
    double _freq;
    /* pitch modulation value */
    double _pitch;
    /* current phase */
    double _phase;
    /* phase increment */
    double _phaseIncrement;
    /* is muted? */
    bool _muted;
    /* holds delay value from leak intregator */
    double _lastOut;
};

#endif
