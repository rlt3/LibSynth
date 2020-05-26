#ifndef AUDIO_DEVICE_HPP
#define AUDIO_DEVICE_HPP

#include <inttypes.h>

class AudioDevice {
public:
    AudioDevice ();
    ~AudioDevice ();

    /* get the samples buffer and its length */
    int16_t* getSamplesBuffer ();
    size_t getNumSamples ();
    size_t getSamplesBytes ();

    /* attempt to play the samples in the samples buffer */
    void playSamples ();

private:
    int16_t* samples;
    /* length of samples buffer in bytes */
    size_t samples_bytes;
    /* how many samples exist */
    size_t num_samples;
    /* */
    long buffer_size;
    /* number of samples per play period */
    long period_size;

    void initDevice ();
    void setupHardware ();
    void setupSoftware ();

    void *device_handle;
};

#endif
