#include "Filter.hpp"
#include "Definitions.hpp"

Filter::Filter (const double cutoff, const double resonance)
    : _mode (FILTER_LOWPASS)
    , _cutoff (0.0)
    , _cutoffThresh (cutoff)
    , _cutoffMod (0.0)
    , _resonance (resonance)
    , _buf0 (0.0)
    , _buf1 (0.0)
    , _buf2 (0.0)
    , _buf3 (0.0)
{
    updateCutoff();
    updateFeedback();
}

double
Filter::process (const double input)
{
    if (input == 0.0)
        return input;
    _buf0 += _cutoff * (input - _buf0 + _feedback * (_buf0 - _buf1));
    _buf1 += _cutoff * (_buf0 - _buf1);
    _buf2 += _cutoff * (_buf1 - _buf2);
    _buf3 += _cutoff * (_buf2 - _buf3);
    switch (_mode) {
        case FILTER_LOWPASS:
            return _buf3;
        case FILTER_HIGHPASS:
            return input - _buf3;
        case FILTER_BANDPASS:
            return _buf0 - _buf3;
        default:
            return 0.0;
    }
}

void
Filter::setCutoff (const double cutoff)
{
    _cutoffThresh = cutoff;
    updateCutoff();
    updateFeedback();
}

void
Filter::setCutoffMod (const double cutoffMod)
{
    _cutoffMod = cutoffMod;
    updateCutoff();
    updateFeedback();
}

void
Filter::setResonance (const double resonance)
{
    _resonance = resonance;
    updateFeedback();
}

void
Filter::setMode (FilterMode mode)
{
    _mode = mode;
}

void inline
Filter::updateCutoff ()
{
    _cutoff = clamp(_cutoffThresh + _cutoffMod, 0.01, 0.99);
}

void inline
Filter::updateFeedback ()
{
    _feedback = _resonance + (_resonance / (1.0 - _cutoff));
}
