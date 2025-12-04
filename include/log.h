#ifndef LOG_H
#define LOG_H

#include <stddef.h>
#include <stdint.h>

// Playback statistics structure
typedef struct {
    // Audio thread stats
    long *audio_runtime_us;      // Time spent in audio processing per cycle
    long *audio_jitter_us;       // Wake jitter (scheduled vs actual)
    long *audio_wake_interval_us; // Actual interval between wakes
    long *audio_buffer_frames;   // Ring buffer fill level (for MP3)
    long *alsa_delay_frames;     // ALSA buffer delay
    size_t audio_samples;
    int underrun_count;
    int buffer_stall_count;      // Times we waited for decoder

    // GPIO/LED thread stats
    long *gpio_write_ns;         // GPIO write duration
    long *gpio_jitter_ns;        // LED thread wake jitter
    size_t gpio_samples;

    // Decoder thread stats (MP3 only)
    long *decode_time_us;        // Time to decode each chunk
    size_t decode_samples;
    int decode_errors;

    // General info
    const char *audio_format;    // "MP3", "WAV", or "NONE"
    uint32_t sample_rate;
    uint16_t channels;
    int pattern_count;
    double playback_duration_sec;
} PlaybackStats;

void save_playback_report(const char *filename, const PlaybackStats *stats);

// Legacy function for compatibility
void save_runtime_log(const char *filename,
		      long *runtimes_us,
		      long *wake_intervals_us,
		      long *jitter_us,
		      size_t runtime_index,
		      int underrun_count);

#endif

