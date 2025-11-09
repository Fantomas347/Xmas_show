#include "load.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define WAV_HEADER_SIZE 44

Pattern patterns[MAX_PATTERNS];
int pattern_count = 0;

void load_wav(const char *filename, uint32_t *sample_rate, uint16_t *channels,
              int16_t *audio_data, size_t *audio_frames, size_t max_frames) {
    FILE *f = fopen(filename, "rb");
    if (!f) { perror("WAV open"); exit(1); }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, WAV_HEADER_SIZE, SEEK_SET);
    long data_size = size - WAV_HEADER_SIZE;

    fseek(f, 24, SEEK_SET); fread(sample_rate, sizeof(uint32_t), 1, f);
    fseek(f, 22, SEEK_SET); fread(channels, sizeof(uint16_t), 1, f);

    *audio_frames = data_size / (*channels * sizeof(int16_t));

    if ((*audio_frames) * (*channels) > max_frames) {
        fprintf(stderr, "WAV too large (%zu frames)\n", *audio_frames);
        exit(1);
    }

    fseek(f, WAV_HEADER_SIZE, SEEK_SET);
    fread(audio_data, 1, data_size, f);
    fclose(f);
}

void load_patterns(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("pattern open"); exit(1); }

    char line[64];
    pattern_count = 0;

    while (fgets(line, sizeof(line), f)) {
        if (pattern_count >= MAX_PATTERNS) {
            fprintf(stderr, "Too many patterns!\n");
            break;
        }
        int dur; char bits[10];
        if (sscanf(line, "%d %9s", &dur, bits) == 2) {
            if (dur < 70) dur = 70;
            dur = ((dur + 5) / 10) * 10;
            uint8_t p = 0;
            for (int i = 0, j = 0; i < 8 && bits[j]; ++j) {
                if (bits[j] == '.') continue;
                p = (p << 1) | (bits[j] == '1' ? 1 : 0);
                ++i;
            }
            patterns[pattern_count++] = (Pattern){dur, p};
        }
    }
    fclose(f);
}
