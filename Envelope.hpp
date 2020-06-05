#ifndef SYNTH_ENVELOPE_HPP
#define SYNTH_ENVELOPE_HPP

typedef enum _EnvelopeStage {
    STAGE_ATTACK = 0,
    STAGE_DECAY,
    STAGE_SUSTAIN,
    STAGE_RELEASE,
    NUM_STAGES,
} EnvelopeStage;

class Envelope {
public:
    Envelope (double ADSR[4]);

    /* Place envelope in ATTACK stage or reset to ATTACK if already on */
    void noteOn ();
    /* Turn off the note placing it in the RELEASE stage */
    void noteOff ();

    /* 
     * returns false if in the RELEASE stage and output level is less than
     * or the minimal level, otherwise true.
     */
    bool isActive () const;

    /* Next sample's envelope level */
    double next ();

    /* update a particular stage's value */
    void setValue (EnvelopeStage stage, double value);

    /* Set the sample rate for all envelopes created */
    static void setRate (unsigned long rate);

protected:
    EnvelopeStage getNextStage () const;

    /* 
     * Calculates a multiplier used to change the output level from `start' to
     * `end' over `_nextStageAt' number of samples. Since ears logarithmically,
     * i.e. exponential changes sound linear to the ear, this calculates the
     * exponential curve between two points. Rather than call `exp' this is
     * a faster optimization.
     */
    void calcStageMultiplier (double start, double end, unsigned long numSamples);

    void enterStage (EnvelopeStage stage);

private:
    const double _minLevel;
    double _level;
    double _multiplier;

    EnvelopeStage _currStage;

    double _values[NUM_STAGES];
    EnvelopeStage _next[NUM_STAGES];

    unsigned long _currSample;
    unsigned long _nextStageAt;

    /* Sample rate for all envelopes */
    static unsigned long rate;
};

#endif
