#include <cassert>
#include <cmath>
#include "Definitions.hpp"
#include "Polyphonic.hpp"

/* PolyNotes start in the active state */
Voice::Voice (enum OscillatorWave wave,
          const double frequency,
          const double velocity,
          double ADSR[4],
          const double cutoff,
          const double resonance,
          double filterADSR[4])
    : _isActive (false)
    , _velocity (0.0)
    , _filter (Filter(cutoff, resonance))
    , _env (Envelope(ADSR))
    , _filterEnv (Envelope(filterADSR))
{
    noteOn(velocity);
    _filter.setMode(FILTER_LOWPASS);
    _oscillator.setMode(wave);
    _oscillator.setFreq(frequency);
    _oscillator.unmute();
}

/* Resets the envelope and note if already active */
void
Voice::noteOn (const double velocity)
{
    _isActive = true;
    _velocity = velocity;
    _env.noteOn();
}

void
Voice::noteOff ()
{
    _env.noteOff();
}

bool
Voice::isActive () const
{
    return _isActive;
}

void
Voice::setWave (enum OscillatorWave wave)
{
    _oscillator.setMode(wave);
}

void
Voice::setPitch (double value)
{
    _oscillator.setPitch(value);
}

void
Voice::setADSR (EnvelopeStage stage, double value)
{
    _env.setValue(stage, value);
}

void
Voice::setFilterCutoff (double value)
{
    _filter.setCutoff(value);
}

void
Voice::setFilterResonance (double value)
{
    _filter.setResonance(value);
}

void
Voice::setFilterADSR (EnvelopeStage stage, double value)
{
    _filterEnv.setValue(stage, value);
}

double
Voice::next ()
{
    assert(_isActive);
    _isActive = _env.isActive();
    _filter.setCutoffMod(_filterEnv.next() * 0.8);
    return _filter.process(_oscillator.next() * _env.next() * _velocity);
}

Polyphonic::Polyphonic (
            double a , double d,  double s,  double r,
            double fa, double fd, double fs, double fr,
            double cutoff, double resonance)
{
    _noteADSR[STAGE_ATTACK] = a;
    _noteADSR[STAGE_DECAY] = d;
    _noteADSR[STAGE_SUSTAIN] = s;
    _noteADSR[STAGE_RELEASE] = r;

    _filterCutoff = cutoff;
    _filterResonance = resonance;
    _filterADSR[STAGE_ATTACK]  = fa;
    _filterADSR[STAGE_DECAY]   = fd;
    _filterADSR[STAGE_SUSTAIN] = fs;
    _filterADSR[STAGE_RELEASE] = fr;
}

void
Polyphonic::noteOn (const int note, const double velocity)
{
    auto it = _notes.find(note);
    if (it != _notes.end()) {
        /* turn note back on if it already exists */
        it->second.noteOn(velocity);
    } else {
        /* otherwise just create it */
        double freq = 440.0 * pow(2.0, (note - 69.0) / 12.0);
        _notes.insert({note, Voice(_waveform, freq, velocity, _noteADSR,
                    _filterCutoff, _filterResonance, _filterADSR)});
    }
}

void
Polyphonic::noteOff (const int note)
{
    auto it = _notes.find(note);
    /* MIDI keyboard sometimes sends errant 'noteOff' events */
    if (it == _notes.end())
        return;
    it->second.noteOff();
}

bool
Polyphonic::noteActive (const int note)
{
    auto it = _notes.find(note);
    if (it == _notes.end())
        return false;
    return it->second.isActive();
}

void
Polyphonic::setWaveForm (enum OscillatorWave wave)
{
    _waveform = wave;
    for (auto it = _notes.begin(); it != _notes.end(); it++)
        it->second.setWave(wave);
}

void
Polyphonic::setPitch (double value)
{
    for (auto it = _notes.begin(); it != _notes.end(); it++)
        it->second.setPitch(value);
}

void
Polyphonic::setADSR (EnvelopeStage stage, double value)
{
    _noteADSR[stage] = value;
    for (auto it = _notes.begin(); it != _notes.end(); it++)
        it->second.setADSR(stage, value);
}

void
Polyphonic::setFilterADSR (EnvelopeStage stage, double value)
{
    _filterADSR[stage] = value;
    for (auto it = _notes.begin(); it != _notes.end(); it++)
        it->second.setFilterADSR(stage, value);
}

void
Polyphonic::setFilterCutoff (double value)
{
    _filterCutoff = value;
    for (auto it = _notes.begin(); it != _notes.end(); it++)
        it->second.setFilterCutoff(value);
}

void
Polyphonic::setFilterResonance (double value)
{
    _filterResonance = value;
    for (auto it = _notes.begin(); it != _notes.end(); it++)
        it->second.setFilterResonance(value);
}

double
Polyphonic::next ()
{
    double out = 0.0;
    /* This weird construction allows removing objects while iterating */
    for (auto it = _notes.begin(); it != _notes.end(); ) {
        if (!it->second.isActive()) {
            if (DEBUG)
                printf("Removing note %2x\n", it->first);
            it = _notes.erase(it);
        } else {
            out += it->second.next();
            it++;
        }
    }
    return out;
}
