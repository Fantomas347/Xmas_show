#include "setup_alsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Target period: 10ms worth of frames
#define AUDIO_PERIOD_MS 10

snd_pcm_t *pcm = NULL;

void setup_alsa(unsigned int sample_rate, unsigned int channels) {
    snd_pcm_hw_params_t *params;
    if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        perror("snd_pcm_open");
        exit(1);
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm, params);
    snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, params, channels);
    snd_pcm_hw_params_set_rate(pcm, params, sample_rate, 0);

    // Calculate period size based on sample rate (10ms worth of frames)
    snd_pcm_uframes_t period_frames = (sample_rate * AUDIO_PERIOD_MS) / 1000;
    snd_pcm_uframes_t buffer_size = period_frames * 12;
    snd_pcm_uframes_t period_size = period_frames;
    snd_pcm_hw_params_set_period_size_near(pcm, params, &period_size, 0);
    snd_pcm_hw_params_set_buffer_size_near(pcm, params, &buffer_size);

    snd_pcm_hw_params(pcm, params);
    snd_pcm_hw_params_free(params);
    snd_pcm_prepare(pcm);

    // --- PRE-FILL ALSA BUFFER WITH SILENCE ---
    int16_t *silence = calloc(period_frames * channels, sizeof(int16_t));
    if (silence) {
        // Write several silent periods to fully flush old data
        for (int i = 0; i < 4; i++) {
            snd_pcm_writei(pcm, silence, period_frames);
        }
        free(silence);
    }

    // Re-prepare device again to reset buffer pointers
    snd_pcm_drop(pcm);
    snd_pcm_prepare(pcm);
}

void alsa_close(void) {
    if (pcm) {
        snd_pcm_drain(pcm);
        snd_pcm_close(pcm);
        pcm = NULL;
    }
}

// --- Mixer control for hardware volume ---

static snd_mixer_t *mixer_handle = NULL;
static snd_mixer_elem_t *mixer_elem = NULL;

int init_mixer(const char *card, const char *selem_name)
{
    int err;
    snd_mixer_selem_id_t *sid;

    if ((err = snd_mixer_open(&mixer_handle, 0)) < 0)
        return err;

    if ((err = snd_mixer_attach(mixer_handle, card)) < 0)
        return err;

    if ((err = snd_mixer_selem_register(mixer_handle, NULL, NULL)) < 0)
        return err;

    if ((err = snd_mixer_load(mixer_handle)) < 0)
        return err;

    snd_mixer_selem_id_malloc(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);

    mixer_elem = snd_mixer_find_selem(mixer_handle, sid);
    snd_mixer_selem_id_free(sid);

    if (!mixer_elem)
        return -1;

    return 0;
}

// volume_percent: 0..100
int set_hw_volume(long volume_percent)
{
    if (!mixer_elem)
        return -1;

    long minv, maxv;
    snd_mixer_selem_get_playback_volume_range(mixer_elem, &minv, &maxv);

    if (volume_percent < 0)   volume_percent = 0;
    if (volume_percent > 100) volume_percent = 100;

    long vol = minv + (maxv - minv) * volume_percent / 100;
    return snd_mixer_selem_set_playback_volume_all(mixer_elem, vol);
}
