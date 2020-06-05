#ifndef SYNTH_POLYPHONIC_HPP
#define SYNTH_POLYPHONIC_HPP

#include <unordered_map>
#include "Oscillator.hpp"
#include "Envelope.hpp"
#include "Filter.hpp"

/*
 * A singlular note.
 */
class Voice {
public:
    Voice (enum OscillatorWave wave,
              const double frequency,
              const double velocity,
              double ADSR[4],
              const double cutoff,
              const double resonance,
              double filterADSR[4]);

    /* See Polyphonic class */
    void noteOn (const double velocity);
    void noteOff ();
    bool isActive () const;
    void setWave (enum OscillatorWave wave);
    void setPitch (double value);
    void setADSR (EnvelopeStage stage, double value);
    void setFilterCutoff (double value);
    void setFilterResonance (double value);
    void setFilterADSR (EnvelopeStage stage, double value);
    double next ();

private:
    bool _isActive;
    double _velocity;
    Filter _filter;
    Envelope _env;
    Envelope _filterEnv;
    Oscillator _oscillator;
};

/*
 * This class handles playing more than one note at a time -- the "many voiced"
 * class.
 */
class Polyphonic {
public:
    /* ADSR and Filter's ADSR + filter's cutoff and resonance */
    Polyphonic (double a,  double d,  double s,  double r,
                double fa, double fd, double fs, double fr,
                double cutoff, double resonance);

    /* Turn a note on and off */
    void noteOn (const int note, const double velocity);
    void noteOff (const int note);

    /* Returns whether a note is currently playing or not */
    bool noteActive (const int note);

    /* Update the waveform for current and future notes */
    void setWaveForm (enum OscillatorWave wave);

    /* Update the pitch for current and future notes */
    void setPitch (double value);

    /* Update the ADSR for current and future notes */
    void setADSR (EnvelopeStage stage, double value);

    /* Update the filter's ADSR for current and future notes */
    void setFilterADSR (EnvelopeStage stage, double value);
    
    /* Update the filter's cutoff for current and future notes */
    void setFilterCutoff (double value);
    
    /* Update the filter's resonance for current and future notes */
    void setFilterResonance (double value);

    /* Get the next sample */
    double next ();

private:
    double _noteADSR[4];
    double _filterADSR[4];
    double _filterResonance;
    double _filterCutoff;
    enum OscillatorWave _waveform;
    std::unordered_map<int, Voice> _notes;
};

#endif
