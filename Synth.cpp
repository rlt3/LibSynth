#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "AudioDevice.hpp"

void
add_sine_wave (double *buffer,
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
        buffer[i] += sin(phase) * 32767.0f * amp;
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

    size_t rate = PCM.getRate();
    size_t period_size = PCM.getPeriodSize();

    size_t samples_len = period_size * 16;
    size_t samples_bytes = samples_len * sizeof(double);
    double *samples = (double*) malloc(samples_bytes);
    double phase = 0;

    while (1) {
        memset(samples, 0, samples_bytes);
		//add_sine_wave(samples, samples_len, rate, 440.0f,  0.50f, &phase);
		add_sine_wave(samples, samples_len, rate, 554.37f, 0.30f, &phase);
		add_sine_wave(samples, samples_len, rate, 659.26f, 0.05f, &phase);
        PCM.play(samples, samples_len);
    }

    free(samples);
    return 0;
}
