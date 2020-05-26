#include <unistd.h>
#include <stdio.h>
#include <cmath>
#include <cstring>
#include "AudioDevice.hpp"

void add_sine_wave (int16_t* buffer,
                    int buffer_length,
                    float freq,
                    float rate,
                    float amp,
                    float *currphase)
{
    static const double max_phase = 2. * M_PI;
    float step = max_phase * freq / (float) rate;
    float phase = *currphase;
    for (int i = 0; i < buffer_length; i++) {
        buffer[i] += static_cast<int16_t>(sin(phase) * 32767.0f * amp);
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

    float phase = 0;
    while (1) {
        memset(samples, 0, samples_bytes);
		add_sine_wave(samples, num_samples, 440.0f,  44100.f, 0.50f, &phase);
		//add_sine_wave(samples, num_samples, 554.37f, 44100.f, 0.30f, &phase);
		//add_sine_wave(samples, num_samples, 659.26f, 44100.f, 0.05f, &phase);
        PCM.playSamples();
    }

    return 0;
}
