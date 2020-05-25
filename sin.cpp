#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <alsa/asoundlib.h>

#define PCM_DEVICE "default"

/* Underrun and suspend recovery */
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

void
generate_sine (int16_t *samples,
               size_t format_width,
               unsigned int rate,
               unsigned int freq,
               int count, 
               double *currphase)
{
    static const double max_phase = 2. * M_PI;
    const size_t maxval = (1 << (format_width - 1)) - 1;
    const int bps = format_width / 8;  /* bytes per sample */

    int8_t *buff = (int8_t*) samples;

    double step = max_phase * freq / (double)rate;
    double phase = *currphase;

    int val, i;
    while (count-- > 0) {
        val = sin(phase) * maxval;

        for (i = 0; i < bps; i++)
            *(buff + i) = (val >> i * 8) & 0xff;
        buff += 2;

        phase += step;
        if (phase >= max_phase)
            phase -= max_phase;
    }

    *currphase = phase;
}

int
main (void)
{
    /* playback device */
    static const char *device = "default";
    /* sample format */
    static snd_pcm_format_t format = SND_PCM_FORMAT_S16;
    static snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;
    static const size_t format_width = snd_pcm_format_physical_width(format);
    /* stream rate */
    static unsigned int rate = 44100;
    /* count of channels */
    static unsigned int channels = 1;
    /* ring buffer length in us */
    static unsigned int buffer_time = 500000;
    /* period time in us */
    static unsigned int period_time = 100000;
    /* sinusoidal wave frequency in Hz */
    static double freq = 440;

    static snd_pcm_sframes_t buffer_size;
    static snd_pcm_sframes_t period_size;

    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    snd_pcm_t *handle;
    snd_pcm_uframes_t val;
    int err;

    int16_t *samples = NULL;

    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 0;
    }

    /* fill hwparams with default values */
    err = snd_pcm_hw_params_any(handle, hwparams);
    if (err < 0) {
        printf("No hardware configurations available: %s\n", snd_strerror(err));
        return err;
    }

    /* enable resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, hwparams, 1);
    if (err < 0) {
        printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }

    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, hwparams, access);
    if (err < 0) {
        printf("Access not available for playback: %s\n", snd_strerror(err));
        return err;
    }

    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, hwparams, format);
    if (err < 0) {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }

    /* set number of channels */
    err = snd_pcm_hw_params_set_channels(handle, hwparams, channels);
    if (err < 0) {
        printf("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }

    /* set the stream rate */
    unsigned rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, hwparams, &rrate, 0);
    if (err < 0) {
        printf("Rate %uHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate) {
        printf("Rate doesn't match (requested %uHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }

    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, hwparams, &buffer_time, NULL);
    if (err < 0) {
        printf("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(hwparams, &val);
    if (err < 0) {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    buffer_size = val;

    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(handle, hwparams, &period_time, NULL);
    if (err < 0) {
        printf("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(hwparams, &val, NULL);
    if (err < 0) {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    period_size = val;

    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, hwparams);
    if (err < 0) {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }

    printf("Period Size: %ld\n", period_size);
    printf("Buffer Size: %ld\n", buffer_size);

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
        printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }

    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
    if (err < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }

    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
    if (err < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }

    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }

    size_t num_samples = (period_size * channels * format_width) / 8;
    samples = (int16_t*) malloc(num_samples);
    printf("Num Samples: %lu\n", num_samples);

    double phase = 0;
    signed short *ptr;
    int cptr;
    while (1) {
        generate_sine(samples, format_width, rate, freq, period_size, &phase);
        ptr = samples;
        cptr = period_size;
        while (cptr > 0) {
            err = snd_pcm_writei(handle, ptr, cptr);
            if (err == -EAGAIN)
                continue;
            if (err < 0) {
                if (xrun_recovery(handle, err) < 0) {
                    printf("Write error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
                break;  /* skip one period */
            }
            ptr += err * channels;
            cptr -= err;
        }
    }

    snd_pcm_close(handle);
    return 0;
}
