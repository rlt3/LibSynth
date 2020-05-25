#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <alsa/asoundlib.h>

#define PCM_DEVICE "default"
#define CHK(stmt, msg) if((stmt) < 0) {puts("ERROR: "#msg); exit(1);}

#define POLY 10
#define BUFFSIZE 512
#define GAIN 5000.0

double phi[POLY], phi_mod[POLY], velocity[POLY], env_time[POLY], env_level[POLY];
int note[POLY], gate[POLY], note_active[POLY];

int harmonic = 3;
int subharmonic = 5;
int transpose = 24; 
double modulation = 7.8;
double pitch = 0.0;
double attack = 0.01;
double decay = 0.8;
double sustain = 0.0;
double release = 0.1;

void
midi_open (snd_seq_t **handle, int *port)
{
    CHK(snd_seq_open(handle, "default", SND_SEQ_OPEN_INPUT, 0),
            "Could not open sequencer");

    CHK(snd_seq_set_client_name(*handle, "Midi Listener"),
            "Could not set client name");
    CHK(*port = snd_seq_create_simple_port(*handle, "listen:in",
                SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                SND_SEQ_PORT_TYPE_APPLICATION),
            "Could not open port");
    CHK(snd_seq_nonblock(*handle, 1),
            "Could not set non-blocking");
}

snd_seq_event_t *
midi_read (snd_seq_t *handle)
{
    snd_seq_event_t *ev = NULL;
    const int r = snd_seq_event_input(handle, &ev);
    if (r == -EAGAIN) {
        return NULL;
    } else if (r < 0) {
		fprintf(stderr, "ERROR: %s\n", snd_strerror(r));
        exit(1);
    }
    return ev;
}

void
midi_process (snd_seq_event_t *ev)
{
    switch (ev->type) {
        case SND_SEQ_EVENT_PITCHBEND:
            printf("[%d] Pitchbend:  val(%2x)\n", ev->time.tick,
                                                  ev->data.control.value);
            pitch = (double)ev->data.control.value / 8192.0;
            break;

        case SND_SEQ_EVENT_CONTROLLER:
            printf("[%d] Control:  %2x val(%2x)\n", ev->time.tick,
                                                    ev->data.control.param,
                                                    ev->data.control.value);
            if (ev->data.control.param == 1) {
                modulation = (double)ev->data.control.value / 10.0;
            } 
            break;

        case SND_SEQ_EVENT_NOTEON:
            printf("[%d] Note on: %2x vel(%2x)\n", ev->time.tick,
                                                   ev->data.note.note,
                                                   ev->data.note.velocity);
            for (int i = 0; i < POLY; i++) {
                if (!note_active[i]) {
                    note[i] = ev->data.note.note;
                    velocity[i] = ev->data.note.velocity / 127.0;
                    env_time[i] = 0;
                    gate[i] = 1;
                    note_active[i] = 1;
                    break;
                }
            }
            break;        

        case SND_SEQ_EVENT_NOTEOFF:
            printf("[%d] Note off: %2x vel(%2x)\n", ev->time.tick,
                                                   ev->data.note.note,
                                                   ev->data.note.velocity);
            //for (int i = 0; i < POLY; i++) {
            //    if (gate[i] && note_active[i] && (note[i] == ev->data.note.note)) {
            //        env_time[i] = 0;
            //        gate[i] = 0;
            //    }
            //}
            break;        
    }
}

double
envelope (int *note_active, int gate, double *env_level, double t,
          double attack, double decay, double sustain, double release)
{

    if (gate)  {
        if (t > attack + decay) return(*env_level = sustain);
        if (t > attack) return(*env_level = 1.0 - (1.0 - sustain) * (t - attack) / decay);
        return(*env_level = t / attack);
    } else {
        if (t > release) {
            if (note_active) *note_active = 0;
            return(*env_level = 0);
        }
        return(*env_level * (1.0 - t / release));
    }
}

void
midi_fill_buff (short *buff, long unsigned buffsize, long unsigned nframes)
{
    double dphi, dphi_mod, f1, f2, f3, freq_note, sound;

    //memset(buff, 0, buffsize);

    for (int i = 0; i < POLY; i++) {
        if (note_active[i]) {
            f1 = 8.176 * exp((double)(transpose+note[i]-2)*log(2.0)/12.0);
            f2 = 8.176 * exp((double)(transpose+note[i])*log(2.0)/12.0);
            f3 = 8.176 * exp((double)(transpose+note[i]+2)*log(2.0)/12.0);
            freq_note = (pitch > 0) ? f2 + (f3-f2)*pitch : f2 + (f2-f1)*pitch;
            dphi = M_PI * freq_note / 22050.0;                                    
            dphi_mod = dphi * (double)harmonic / (double)subharmonic;
            /* there are nframes (1024). buffsize is 4096 because nframes * 2
             * channels * 2 bytes per channel. 2 bytes per channel is why we're
             * using a short buffer. since buff is accessed by a factor of 2,
             * looping to nframes yields max of 2047 which is correct because
             * 2048 * 2 bytes is size of buff.
             */
            for (unsigned j = 0; j < nframes; j++) {
                phi[i] += dphi;
                phi_mod[i] += dphi_mod;
                if (phi[i] > 2.0 * M_PI) phi[i] -= 2.0 * M_PI;
                if (phi_mod[i] > 2.0 * M_PI) phi_mod[i] -= 2.0 * M_PI;
      
                sound = GAIN * envelope(&note_active[i], gate[i], &env_level[i], env_time[i], attack, decay, sustain, release) * velocity[i] * sin(phi[i] + modulation * sin(phi_mod[i]));
                env_time[i] += 1.0 / 44100.0;
                buff[2 * j] += sound;
                buff[2 * j + 1] += sound;
            }
        }    
    }
}

void
open_pcm (int channels,
          unsigned int rate,
          snd_pcm_t **handle,
          snd_pcm_uframes_t *frames,
          int *buff_size)
{
	snd_pcm_hw_params_t *params;
	snd_pcm_sw_params_t *sw_params;
    unsigned int tmp;
    unsigned int r;

	/* Open the PCM device in playback mode */
	r = snd_pcm_open(handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
	if (r < 0) 
		printf("ERROR: Can't open \"%s\" PCM device. %s\n",
					PCM_DEVICE, snd_strerror(r));

	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(*handle, params);

	/* Set parameters */
	r = snd_pcm_hw_params_set_access(*handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (r < 0)
		printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(r));

	r = snd_pcm_hw_params_set_format(*handle, params, SND_PCM_FORMAT_S16_LE);
    if (r < 0)
		printf("ERROR: Can't set format. %s\n", snd_strerror(r));

	r = snd_pcm_hw_params_set_channels(*handle, params, channels);
    if (r < 0)
		printf("ERROR: Can't set channels number. %s\n", snd_strerror(r));

	r = snd_pcm_hw_params_set_rate_near(*handle, params, &rate, 0);
    if (r < 0)
		printf("ERROR: Can't set rate. %s\n", snd_strerror(r));

    //snd_pcm_hw_params_set_periods(*handle, params, 2, 0);
    //snd_pcm_hw_params_set_period_size(*handle, params, BUFFSIZE, 0);

	/* Write parameters */
	r = snd_pcm_hw_params(*handle, params);
	if (r < 0)
		printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(r));

	//snd_pcm_sw_params_alloca(&sw_params);
    //snd_pcm_sw_params_current(*handle, sw_params);
    //snd_pcm_sw_params_set_avail_min(*handle, sw_params, BUFFSIZE);
    //snd_pcm_sw_params(*handle, sw_params);

	/* Resume information */
	printf("PCM name: '%s'\n", snd_pcm_name(*handle));
	printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(*handle)));
	snd_pcm_hw_params_get_channels(params, &tmp);
	printf("channels: %i ", tmp);

	if (tmp == 1)
		printf("(mono)\n");
	else if (tmp == 2)
		printf("(stereo)\n");

	snd_pcm_hw_params_get_rate(params, &tmp, 0);
	printf("rate: %d bps\n", tmp);

	/* Allocate buffer to hold single period */
	snd_pcm_hw_params_get_period_size(params, frames, 0);

    /*
     * buffsize = nframes * 2 channels * 2 bytes/sample;
     * This means one frame has 4 bytes dedicated to it, or 2 bytes (16 bits)
     * per channel.
     */

	*buff_size = *frames * channels * 2 /* 2 -> sample size */;
    printf("buff_size: %d\n", *buff_size);
	//snd_pcm_hw_params_get_period_time(params, &tmp, NULL);
}

void
close_pcm (snd_pcm_t *handle, short *buff)
{
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
    if (buff)
        free(buff);
}

int
main (int argc, char **argv)
{
    const int channels = 2;
    const unsigned int rate = 44100;

	snd_pcm_t *pcm_handle;
	snd_pcm_uframes_t frames;
    int buff_size;
    short *buff = NULL;
    int r;

    snd_seq_t *seq_handle;
    int in_port;

    open_pcm(channels, rate, &pcm_handle, &frames, &buff_size);
    printf("frames: %ld\n", frames);

    int16_t buffer[44100];
    float frequency = 440.0f;
    float sampling_ratio = 44100.0f;
    float amplitude = 0.5f;
    float t;
    for (int i = 0; i < 44100; i++) {
        float theta = ((float)i / sampling_ratio) * M_PI;
        buffer[i] = (int16_t)(sin(theta * frequency) * 32767.0f * amplitude);
    }

    r = snd_pcm_writei(pcm_handle, buffer, 1);
    if (r == -EPIPE) {
        printf("XRUN.\n");
        snd_pcm_prepare(pcm_handle);
    }

    sleep(1);

	//buff = (short *) malloc(buff_size);

    //for (int i = 0; i < buff_size / 2; i++) {
    //    //buff[i] = 128 + floor(128.0 * sin(i / 512.0));
    //    buff[i] = 12000 * floor(sin(i / 512.0));
    //}

    //FILE *f = fopen("tmp.bin", "rb");
    //while (1) {
    //    /* if failed to read bytes, set file to beginning */
    //    if (fread(buff, 4096, 1, f) <= 0) {
    //        fseek(f, 4096, SEEK_SET);
    //        continue;
    //    }
    //    r = snd_pcm_writei(pcm_handle, buff, frames);
    //    if (r == -EPIPE) {
    //        printf("XRUN.\n");
    //        snd_pcm_prepare(pcm_handle);
    //    } else if (r < 0) {
    //        printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(r));
    //    }
    //    usleep(100);
    //}
    //fclose(f);

    //FILE *f = fopen("tmp.bin", "wb");
    //midi_open(&seq_handle, &in_port);
    //memset(buff, 0, buff_size);
    //snd_seq_event_t *e = NULL;
    //bool had_input = false;
    //while (1) {
    //    r = snd_seq_event_input(seq_handle, &e);
    //    if (r != -EAGAIN) {
    //        had_input = true;
    //        midi_process(e);
    //        snd_seq_free_event(e);
    //        //r = snd_seq_event_input(seq_handle, &e);
    //    }

    //    //midi_process(midi_read(seq_handle));
    //    //midi_process(seq_handle);
    //    if (had_input) {
    //        fwrite(buff, 4096, 1, f);
    //        //had_input = false;
    //    }

    //    midi_fill_buff(buff, buff_size, frames);

    //    /* TODO  
    //     * need a non-blocking loop with buffer init'd to zero such that we can
    //     * fill pcm with proper amount of data.
    //     */
    //    r = snd_pcm_writei(pcm_handle, buff, frames);
    //    if (r == -EPIPE) {
    //        printf("XRUN.\n");
    //        snd_pcm_prepare(pcm_handle);
    //    } else if (r < 0) {
    //        printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(r));
    //    }

    //    //for (int i = 0; i < buff_size / 2; i++) {
    //    //    buff[i] = 0;
    //    //    //buff[i] = (buff[i] + 64) % 32767;
    //    //}

    //    /* TODO 
    //     * need to calculate microseconds to sleep based on buffsize delivered
    //     * to pcm. so, how many microseconds should I sleep for 1024 frames 
    //     * (2 bytes per sample, 2 samples per channel)?
    //     */
    //    //usleep(100);
    //}
    //fclose(f);

    close_pcm(pcm_handle, buff);
    return 0;
}
