#ifndef SYNTH_FILTER_HPP
#define SYNTH_FILTER_HPP

typedef enum _FilterMode {
    FILTER_LOWPASS = 0,
    FILTER_HIGHPASS,
    FILTER_BANDPASS,
} FilterMode;

/*
 * Low/Hi/Bandpass filter
 */
class Filter {
public:
    Filter (const double cutoff, const double resonance);

    double process (const double input);
    void setCutoff (const double cutoff);
    void setCutoffMod (const double cutoffMod);
    void setResonance (const double resonance);
    void setMode (FilterMode mode);

protected:
    void inline updateCutoff ();
    void inline updateFeedback ();

private:
    FilterMode _mode;

    /* actual cutoff used when filtering */
    double _cutoff;
    /* used as the cutoff threshold when adding the modulation */
    double _cutoffThresh;
    /* modulation from an envelope or whatever else */
    double _cutoffMod;
    double _resonance;
    double _feedback;
    /* four filter accumulators in series */
    double _buf0;
    double _buf1;
    double _buf2;
    double _buf3;
};

#endif
