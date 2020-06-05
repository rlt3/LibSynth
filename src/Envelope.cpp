#include <cmath>
#include "Envelope.hpp"
#include "Definitions.hpp"

Envelope::Envelope (double ADSR[4])
    : _minLevel (0.0001)
    , _level (_minLevel)
    , _multiplier (1.0) 
    , _currStage (STAGE_ATTACK)
    , _currSample (0)
    , _nextStageAt (0)
{
    /* determines when the next stage occurs */
    _values[STAGE_ATTACK]  = ADSR[0];
    _values[STAGE_DECAY]   = ADSR[1];
    _values[STAGE_SUSTAIN] = ADSR[2];
    _values[STAGE_RELEASE] = ADSR[3];

    /* simple state transition table */
    _next[STAGE_ATTACK]  = STAGE_DECAY;
    _next[STAGE_DECAY]   = STAGE_SUSTAIN;
    _next[STAGE_SUSTAIN] = STAGE_SUSTAIN;
    _next[STAGE_RELEASE] = STAGE_RELEASE;
}

void
Envelope::noteOn ()
{
    enterStage(STAGE_ATTACK);
}

void
Envelope::noteOff ()
{
    enterStage(STAGE_RELEASE);
}

bool
Envelope::isActive () const
{
    if (_currStage == STAGE_RELEASE && _level <= _minLevel)
        return false;
    return true;
}

double
Envelope::next ()
{
    if (_currStage != STAGE_SUSTAIN) {
        if (_currSample == _nextStageAt)
            enterStage(getNextStage());
        _level *= _multiplier;
        _currSample++;
    }
    return _level;
}

void
Envelope::setValue (EnvelopeStage stage, double value)
{
    _values[stage] = value;
    if (_currStage != stage)
        return;
    if (_currStage == STAGE_SUSTAIN) {
        _level = value;
    }
    else if (_currStage == STAGE_DECAY && stage == STAGE_SUSTAIN) {
        unsigned long samplesLeft = _nextStageAt - _currSample;
        calcStageMultiplier(_level, std::max(value, _minLevel), samplesLeft);
    }
    else {
        double nextLevel = _minLevel;
        switch (_currStage) {
            case STAGE_ATTACK:
                nextLevel = 1.0;
                break;
            case STAGE_DECAY:
                nextLevel = std::max(_values[STAGE_SUSTAIN], _minLevel);
                break;
            case STAGE_RELEASE:
                nextLevel = _minLevel;
                break;
            default:
                break;
        }

        double percentDone = (double) _currSample / (double) _nextStageAt;
        double percentLeft = 1.0 - percentDone;
        unsigned long samplesLeft = percentLeft * value * Envelope::rate;
        _nextStageAt = _currSample + samplesLeft;
        calcStageMultiplier(_level, nextLevel, samplesLeft);
    }
}

EnvelopeStage
Envelope::getNextStage () const
{
    return _next[_currStage];
}

/* 
 * Calculates a multiplier used to change the output level from `start' to
 * `end' over `_nextStageAt' number of samples. Since ears logarithmically,
 * i.e. exponential changes sound linear to the ear, this calculates the
 * exponential curve between two points. Rather than call `exp' this is
 * a faster optimization.
 */
void
Envelope::calcStageMultiplier (double start, double end, unsigned long numSamples)
{
    _multiplier = 1.0 + (log(end) - log(start)) / numSamples;
}

void
Envelope::enterStage (EnvelopeStage stage)
{
    _currStage = stage;

    _currSample = 0;
    if (_currStage == STAGE_SUSTAIN)
        _nextStageAt = 0;
    else
        _nextStageAt = _values[_currStage] * Envelope::rate;

    switch (_currStage) {
        case STAGE_ATTACK:
            _level = _minLevel;
            calcStageMultiplier(_level, 1.0, _nextStageAt);
            break;

        case STAGE_DECAY:
            _level = 1.0;
            calcStageMultiplier(_level,
                    std::max(_values[STAGE_SUSTAIN], _minLevel),
                    _nextStageAt);
            break;

        case STAGE_SUSTAIN:
            _level = _values[STAGE_SUSTAIN];
            _multiplier = 1.0;
            break;

        case STAGE_RELEASE:
            /* 
             * Because this stage can be entered by any stage by releasing
             * the key, let it `decay' from the current output level.
             */
            calcStageMultiplier(_level, _minLevel, _nextStageAt);
            break;

        default:
            break;
    }
}

/* Set the sample rate for all envelopes created */
void
Envelope::setRate (unsigned long rate)
{
    Envelope::rate = rate;
}

unsigned long Envelope::rate = 44100;
