#ifndef SETUP_ALSA_H
#define SETUP_ALSA_H

#include <alsa/asoundlib.h>
#include <stdint.h>

extern snd_pcm_t *pcm;

void setup_alsa(unsigned int sample_rate, unsigned int channels);
void alsa_close(void);

#endif
