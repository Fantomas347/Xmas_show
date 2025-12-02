#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

// Ring buffer size: ~3 seconds at 48000Hz stereo (16-bit)
#define RING_BUFFER_FRAMES  (48000 * 3)
#define RING_BUFFER_SAMPLES (RING_BUFFER_FRAMES * 2)  // stereo

typedef enum {
    AUDIO_FORMAT_UNKNOWN,
    AUDIO_FORMAT_WAV,
    AUDIO_FORMAT_MP3
} AudioFormat;

typedef struct {
    // Audio properties
    uint32_t sample_rate;
    uint16_t channels;
    size_t total_frames;      // Total frames in file (0 if unknown/streaming)

    // Ring buffer for streaming
    int16_t *ring_buffer;
    size_t ring_size;         // Total size in samples
    volatile size_t write_pos; // Producer position (decoder thread)
    volatile size_t read_pos;  // Consumer position (audio thread)
    volatile int finished;     // Decoder has finished
    volatile int error;        // Error occurred

    // Threading
    pthread_t decoder_thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond_space;  // Signal when space available
    pthread_cond_t cond_data;   // Signal when data available
    int thread_running;

    // Format-specific handles
    AudioFormat format;
    void *decoder_handle;     // mpg123_handle* for MP3

    // File info (for WAV fallback or file handle)
    int fd;
    void *mapping;            // mmap for WAV
    size_t mapping_size;
    int16_t *wav_pcm;         // Direct PCM pointer for WAV
    size_t wav_frames_read;   // Current position for WAV streaming
} AudioStream;

// Open audio file (detects format by extension)
AudioStream *audio_open(const char *filename);

// Start decoder thread (for MP3) or prepare WAV
int audio_start(AudioStream *stream);

// Read samples from stream (called by audio thread)
// Returns number of frames read, 0 if buffer empty, -1 if finished
int audio_read(AudioStream *stream, int16_t *buffer, size_t frames);

// Check if stream has finished
int audio_finished(AudioStream *stream);

// Get available frames in buffer
size_t audio_available(AudioStream *stream);

// Close and free stream
void audio_close(AudioStream *stream);

#endif
