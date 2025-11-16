#include "load.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    char     riff_id[4];   // "RIFF"
    uint32_t riff_size;
    char     wave_id[4];   // "WAVE"
} RiffHeader;

typedef struct {
    char     chunk_id[4];  // e.g. "fmt " or "data"
    uint32_t chunk_size;
} ChunkHeader;

typedef struct {
    uint16_t audio_format;   // 1 = PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} FmtChunk;
#pragma pack(pop)

Pattern patterns[MAX_PATTERNS];
int pattern_count = 0;

void load_wav(const char *filename, uint32_t *sample_rate, uint16_t *channels,
              int16_t *audio_data, size_t *audio_frames, size_t max_frames)
{
    FILE *f = fopen(filename, "rb");
    if (!f) { perror("WAV open"); exit(1); }

    RiffHeader riff;
    if (fread(&riff, sizeof(RiffHeader), 1, f) != 1) {
        fprintf(stderr, "Invalid WAV header (RIFF)\n");
        exit(1);
    }
    if (memcmp(riff.riff_id, "RIFF", 4) != 0 || memcmp(riff.wave_id, "WAVE", 4) != 0) {
        fprintf(stderr, "Not a RIFF/WAVE file\n");
        exit(1);
    }

    FmtChunk fmt = {0};
    uint32_t data_size = 0;
    long     data_offset = 0;

    // Scan chunks until we find "fmt " and "data"
    while (!feof(f)) {
        ChunkHeader ch;
        if (fread(&ch, sizeof(ChunkHeader), 1, f) != 1)
            break;

        if (memcmp(ch.chunk_id, "fmt ", 4) == 0) {
            if (ch.chunk_size < sizeof(FmtChunk)) {
                fprintf(stderr, "fmt chunk too small\n");
                exit(1);
            }
            if (fread(&fmt, sizeof(FmtChunk), 1, f) != 1) {
                fprintf(stderr, "Failed to read fmt chunk\n");
                exit(1);
            }
            // Skip any extra fmt bytes if present
            if (ch.chunk_size > sizeof(FmtChunk)) {
                fseek(f, ch.chunk_size - sizeof(FmtChunk), SEEK_CUR);
            }
        } else if (memcmp(ch.chunk_id, "data", 4) == 0) {
            data_size = ch.chunk_size;
            data_offset = ftell(f);
            // this is where the PCM data actually starts
            break;
        } else {
            // Skip unknown chunk
            fseek(f, ch.chunk_size, SEEK_CUR);
        }
    }

    if (data_size == 0 || data_offset == 0) {
        fprintf(stderr, "No data chunk in WAV\n");
        exit(1);
    }

    if (fmt.audio_format != 1 || fmt.bits_per_sample != 16) {
        fprintf(stderr, "Unsupported WAV format (need PCM 16-bit)\n");
        exit(1);
    }

    *sample_rate = fmt.sample_rate;
    *channels    = fmt.num_channels;

    size_t frames = data_size / (fmt.num_channels * sizeof(int16_t));
    if (frames > max_frames) {
        fprintf(stderr, "WAV too large (%zu frames)\n", frames);
        exit(1);
    }

    // Read PCM data into buffer
    fseek(f, data_offset, SEEK_SET);
    size_t read = fread(audio_data, sizeof(int16_t) * fmt.num_channels, frames, f);
    if (read != frames) {
        fprintf(stderr, "Short read: expected %zu frames, got %zu\n", frames, read);
    }

    *audio_frames = read;
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
