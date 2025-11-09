#ifndef LOAD_H
#define LOAD_H

#include <stdint.h>
#include <stddef.h>

#define MAX_PATTERNS 2048

typedef struct {
	int duration_ms;
	uint8_t pattern;
} Pattern;

extern Pattern patterns[MAX_PATTERNS];
extern int pattern_count;

void load_wav(const char *filename, uint32_t *sample_rate, uint16_t *channels, int16_t *audio_data, size_t *audio_frames, size_t max_frames);
void load_patterns(const char *filename);

#endif
