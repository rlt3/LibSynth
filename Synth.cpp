#include <unistd.h>
#include <stdio.h>
#include <cmath>
#include <cstring>
#include "AudioDevice.hpp"

static inline int16_t
clip (double v)
{
    int32_t m = static_cast<int32_t>(v);
    if (m > INT16_MAX)
        m = INT16_MAX;
    else if (m < INT16_MIN)
        m = INT16_MIN;
    return static_cast<int16_t>(m);
}

void
add_sine_wave (int16_t* buffer,
               int buffer_length,
               double rate,
               double freq,
               double amp,
               double *currphase)
{
    static const double max_phase = 2. * M_PI;
    double step = max_phase * freq / rate;
    double phase = *currphase;

    for (int i = 0; i < buffer_length; i++) {
        buffer[i] += clip(sin(phase) * 32767.0f * amp);

        phase += step;
        if (phase >= max_phase)
            phase -= max_phase;
    }

    *currphase = phase;
}

int
main ()
{
    AudioDevice PCM;

    int16_t* samples = PCM.getSamplesBuffer();
    size_t samples_bytes = PCM.getSamplesBytes();
    size_t num_samples = PCM.getNumSamples();
    unsigned int rate = PCM.getRate();

    double phase = 0;
    while (1) {
        memset(samples, 0, samples_bytes);
		add_sine_wave(samples, num_samples, rate, 440.0f, 0.50f, &phase);
		//add_sine_wave(samples, num_samples, rate, 554.37f, 0.30f, &phase);
		//add_sine_wave(samples, num_samples, rate, 659.26f, 0.05f, &phase);
        PCM.playSamples();
    }

    return 0;
}
