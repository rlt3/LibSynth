#include "Definitions.hpp"
#include "Synth.hpp"

Synth::Synth ()
{
    init(NULL);
}

Synth::Synth (const char *midiDevice)
{
    init(midiDevice);
}

Synth::Synth (const std::string midiDevice)
{
    init(midiDevice.c_str());
}

Synth::~Synth ()
{
    _running = false;
    pthread_join(_thread, NULL);
    delete _audio;
    delete _midi;
    delete _polyphonic;
    delete[] _samples;
}

void
Synth::setVolume (const double value)
{
    _volume = clamp(value, 0.0, 1.5);
}

void
Synth::setWaveform (const OscillatorWave wave)
{
    _polyphonic->setWaveForm(wave);
}

void
Synth::setAttack (const double value) const
{
    _polyphonic->setADSR(STAGE_ATTACK, clamp(value, 0.01, 1.5));
}

void
Synth::setDecay (const double value) const
{
    _polyphonic->setADSR(STAGE_DECAY, clamp(value, 0.01, 1.5));
}

void
Synth::setSustain (const double value) const
{
    _polyphonic->setADSR(STAGE_SUSTAIN, clamp(value, 0.01, 1.5));
}

void
Synth::setRelease (const double value) const
{
    _polyphonic->setADSR(STAGE_RELEASE, clamp(value, 0.01, 1.5));
}

void
Synth::setCutoff (const double value) const
{
    _polyphonic->setFilterCutoff(clamp(value, 0.0, 0.99));
}

void
Synth::setResonance (const double value) const
{
    _polyphonic->setFilterResonance(clamp(value, 0.0, 0.99));
}

void
Synth::setFilterAttack (const double value) const
{
    _polyphonic->setFilterADSR(STAGE_ATTACK, clamp(value, 0.01, 1.5));
}

void
Synth::setFilterDecay (const double value) const
{
    _polyphonic->setFilterADSR(STAGE_DECAY, clamp(value, 0.01, 1.5));
}

void
Synth::setFilterSustain (const double value) const
{
    _polyphonic->setFilterADSR(STAGE_SUSTAIN, clamp(value, 0.01, 1.5));
}

void
Synth::setFilterRelease (const double value) const
{
    _polyphonic->setFilterADSR(STAGE_RELEASE, clamp(value, 0.01, 1.5));
}

void
Synth::noteOn (const int note, const double velocity) const
{
    _midi->input(MidiEvent(MIDI_NOTEON, note, 0.0, clamp(velocity, 0.0, 1.0), 0.0));
}

void
Synth::noteOff (const int note) const
{
    _midi->input(MidiEvent(MIDI_NOTEOFF, note, 0.0, 0.0, 0.0));
}

bool
Synth::noteActive (const int note) const
{
    return _polyphonic->noteActive(note);
}

void
Synth::init (const char *midiDevice)
{
    _volume = 1.0;
    _audio = new AudioDevice();

    size_t rate = _audio->getRate();
    _samplesLen = _audio->getPeriodSamples();
    _samples = new int16_t[_samplesLen];

    Oscillator::setRate(rate);
    Envelope::setRate(rate);
    _midi = new MidiController(midiDevice);

    /* 
     * A simple default. Short attack, medium decay and sustain, long
     * release. The filter's ADSR should produce a 'tingy' sound with no
     * resonance and a high cutoff.
     */
    _polyphonic = new Polyphonic(
                        0.01, 0.5, 0.5, 1.0,
                        0.2, 0.2, 1.0, 1.0,
                        0.99, 0.0);
    _polyphonic->setWaveForm(OSCILLATOR_WAVE_SQUARE);

    _running = true;
    CHK(pthread_create(&_thread, NULL, Synth::audio_thread, this),
            "Could not create audio thread");
}

/* 
 * Converts a double value into a 16bit signed integer value, clipping any
 * out-of-range values.
 */
static inline int16_t
clip (double x)
{
    if (x > 1.0)
        x = 1.0;
    else if (x < -1.0)
        x = -1.0;
    return static_cast<int16_t>(32767.0 * x);
}

void*
Synth::audio_thread (void *data)
{
    Synth *synth = (Synth*) data;
    Polyphonic *polyphonic = synth->_polyphonic;
    MidiController *midi = synth->_midi;
    AudioDevice *audio = synth->_audio;
    int16_t *samples = synth->_samples;
    size_t samplesLen = synth->_samplesLen;

    while (synth->_running) {
        for (unsigned i = 0; i < samplesLen; i += 2) {
            MidiEvent e = midi->nextEvent();
            switch (e.type) {
                case MIDI_NOTEON:
                    polyphonic->noteOn(e.note, e.velocity);
                    break;
                case MIDI_NOTEOFF:
                    polyphonic->noteOff(e.note);
                    break;
                case MIDI_PITCHBEND:
                    polyphonic->setPitch(e.pitch);
                    break;
                case MIDI_CONTROL:
                    if (e.note <= 4)
                        /* minus 1 because control params start at 1 */
                        polyphonic->setADSR((EnvelopeStage)(e.note - 1), e.control);
                    else if (e.note == 5)
                        polyphonic->setFilterCutoff(e.control);
                    else if (e.note == 6)
                        polyphonic->setFilterResonance(e.control);
                    else if (e.note <= 10)
                        /* minus 6 because stages are 1 through 4 */
                        polyphonic->setFilterADSR((EnvelopeStage)(e.note - 6), e.control);
                    break;
                default:
                    break;
            }
            samples[i] = samples[i + 1] = clip(synth->_volume * polyphonic->next());
        }
        audio->play(samples, samplesLen);
    }

    return NULL;
}
