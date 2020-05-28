#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "AudioDevice.hpp"
#include "Oscillator.hpp"

static int16_t
clip (double x)
{
    if (x > 1.0)
        x = 1.0;
    else if (x < -1.0)
        x = -1.0;
    return 32767.0 * x;
}

int
main ()
{
    AudioDevice PCM;
    Oscillator oscillator;

    size_t period_size = PCM.getPeriodSize();
    size_t samples_len = period_size * 8;
    size_t samples_bytes = samples_len * sizeof(double);
    double *samples = (double*) malloc(samples_bytes);

    oscillator.setRate(PCM.getRate());
    oscillator.setFreq(440.0);

    while (1) {
        for (unsigned i = 0; i < samples_len; ++i) {
            samples[i] = clip(oscillator.next());
        }
        PCM.play(samples, samples_len);
    }

    free(samples);
    return 0;
}
