#ifndef SETUP_ALSA_H
#define SETUP_ALSA_H

#include <alsa/asoundlib.h>
#include <stdint.h>

extern snd_pcm_t *pcm;

void setup_alsa(unsigned int sample_rate, unsigned int channels);
void alsa_close(void);

int init_mixer(const char *card, const char *selem_name);
int set_hw_volume(long volume_percent);

#endif
