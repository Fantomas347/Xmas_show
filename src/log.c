#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Helper to compute statistics
static void compute_stats(long *data, size_t count,
                          long *min, long *max, double *avg, long *p99) {
    if (count == 0) {
        *min = *max = *p99 = 0;
        *avg = 0.0;
        return;
    }

    *min = data[0];
    *max = data[0];
    long sum = 0;

    for (size_t i = 0; i < count; i++) {
        if (data[i] < *min) *min = data[i];
        if (data[i] > *max) *max = data[i];
        sum += data[i];
    }
    *avg = (double)sum / count;

    // Simple p99: sort would be expensive, use approximation
    // Count values above threshold
    long threshold = (long)(*avg * 3);
    int above = 0;
    long max_below = *min;
    for (size_t i = 0; i < count; i++) {
        if (data[i] > threshold) above++;
        else if (data[i] > max_below) max_below = data[i];
    }
    *p99 = (above < (int)(count / 100)) ? max_below : *max;
}

void save_playback_report(const char *filename, const PlaybackStats *stats) {
    FILE *f = fopen(filename, "w");
    if (!f) { perror("playback report fopen"); return; }

    // Header with timestamp
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    fprintf(f, "================================================================================\n");
    fprintf(f, "V43 SEQUENCER PLAYBACK REPORT\n");
    fprintf(f, "Generated: %04d-%02d-%02d %02d:%02d:%02d\n",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    fprintf(f, "================================================================================\n\n");

    // General info
    fprintf(f, "PLAYBACK INFO\n");
    fprintf(f, "-------------\n");
    fprintf(f, "Audio format:      %s\n", stats->audio_format);
    fprintf(f, "Sample rate:       %u Hz\n", stats->sample_rate);
    fprintf(f, "Channels:          %u\n", stats->channels);
    fprintf(f, "Pattern count:     %d\n", stats->pattern_count);
    fprintf(f, "Duration:          %.2f sec\n\n", stats->playback_duration_sec);

    // Audio thread statistics
    if (stats->audio_samples > 0) {
        fprintf(f, "AUDIO THREAD STATISTICS (%zu samples)\n", stats->audio_samples);
        fprintf(f, "--------------------------------------\n");

        long min, max, p99;
        double avg;

        compute_stats(stats->audio_runtime_us, stats->audio_samples, &min, &max, &avg, &p99);
        fprintf(f, "Processing time:   min=%ld us, max=%ld us, avg=%.1f us, p99=%ld us\n",
                min, max, avg, p99);

        compute_stats(stats->audio_jitter_us, stats->audio_samples, &min, &max, &avg, &p99);
        fprintf(f, "Wake jitter:       min=%ld us, max=%ld us, avg=%.1f us, p99=%ld us\n",
                min, max, avg, p99);

        compute_stats(stats->audio_wake_interval_us, stats->audio_samples, &min, &max, &avg, &p99);
        fprintf(f, "Wake interval:     min=%ld us, max=%ld us, avg=%.1f us (target=30000 us)\n",
                min, max, avg);

        if (stats->alsa_delay_frames) {
            compute_stats(stats->alsa_delay_frames, stats->audio_samples, &min, &max, &avg, &p99);
            fprintf(f, "ALSA buffer:       min=%ld, max=%ld, avg=%.0f frames\n",
                    min, max, avg);
        }

        if (stats->audio_buffer_frames) {
            compute_stats(stats->audio_buffer_frames, stats->audio_samples, &min, &max, &avg, &p99);
            fprintf(f, "Ring buffer:       min=%ld, max=%ld, avg=%.0f frames\n",
                    min, max, avg);
        }

        fprintf(f, "Underruns:         %d\n", stats->underrun_count);
        fprintf(f, "Buffer stalls:     %d\n\n", stats->buffer_stall_count);

        // Quality assessment
        fprintf(f, "AUDIO QUALITY ASSESSMENT\n");
        fprintf(f, "------------------------\n");
        if (stats->underrun_count == 0) {
            fprintf(f, "[OK] No underruns detected\n");
        } else if (stats->underrun_count < 5) {
            fprintf(f, "[WARN] %d underruns detected - minor audio glitches possible\n",
                    stats->underrun_count);
        } else {
            fprintf(f, "[FAIL] %d underruns detected - audio quality degraded\n",
                    stats->underrun_count);
        }

        compute_stats(stats->audio_jitter_us, stats->audio_samples, &min, &max, &avg, &p99);
        if (max < 5000) {
            fprintf(f, "[OK] Scheduling jitter within limits (max %ld us)\n", max);
        } else if (max < 15000) {
            fprintf(f, "[WARN] Scheduling jitter elevated (max %ld us)\n", max);
        } else {
            fprintf(f, "[FAIL] Scheduling jitter too high (max %ld us) - RT issues\n", max);
        }

        if (stats->buffer_stall_count == 0) {
            fprintf(f, "[OK] No decoder stalls\n");
        } else {
            fprintf(f, "[WARN] %d decoder stalls - MP3 decoding may be too slow\n",
                    stats->buffer_stall_count);
        }
        fprintf(f, "\n");
    }

    // GPIO/LED thread statistics
    if (stats->gpio_samples > 0) {
        fprintf(f, "LED THREAD STATISTICS (%zu samples)\n", stats->gpio_samples);
        fprintf(f, "-----------------------------------\n");

        long min, max, p99;
        double avg;

        compute_stats(stats->gpio_write_ns, stats->gpio_samples, &min, &max, &avg, &p99);
        fprintf(f, "GPIO write time:   min=%.2f us, max=%.2f us, avg=%.2f us\n",
                min / 1000.0, max / 1000.0, avg / 1000.0);

        compute_stats(stats->gpio_jitter_ns, stats->gpio_samples, &min, &max, &avg, &p99);
        fprintf(f, "Wake jitter:       min=%.2f us, max=%.2f us, avg=%.2f us, p99=%.2f us\n",
                min / 1000.0, max / 1000.0, avg / 1000.0, p99 / 1000.0);

        fprintf(f, "\nLED QUALITY ASSESSMENT\n");
        fprintf(f, "----------------------\n");
        if (max / 1000.0 < 1000) {
            fprintf(f, "[OK] LED timing jitter within limits (max %.2f us)\n", max / 1000.0);
        } else if (max / 1000.0 < 5000) {
            fprintf(f, "[WARN] LED timing jitter elevated (max %.2f us)\n", max / 1000.0);
        } else {
            fprintf(f, "[FAIL] LED timing jitter too high (max %.2f us)\n", max / 1000.0);
        }
        fprintf(f, "\n");
    }

    // CSV data section
    fprintf(f, "================================================================================\n");
    fprintf(f, "RAW DATA (CSV format)\n");
    fprintf(f, "================================================================================\n\n");

    // Audio data
    if (stats->audio_samples > 0) {
        fprintf(f, "# Audio thread data\n");
        fprintf(f, "audio_index,runtime_us,jitter_us,wake_interval_us,alsa_delay,ring_buffer\n");
        for (size_t i = 0; i < stats->audio_samples; i++) {
            fprintf(f, "%zu,%ld,%ld,%ld,%ld,%ld\n",
                    i,
                    stats->audio_runtime_us[i],
                    stats->audio_jitter_us[i],
                    stats->audio_wake_interval_us[i],
                    stats->alsa_delay_frames ? stats->alsa_delay_frames[i] : 0,
                    stats->audio_buffer_frames ? stats->audio_buffer_frames[i] : 0);
        }
        fprintf(f, "\n");
    }

    // GPIO data
    if (stats->gpio_samples > 0) {
        fprintf(f, "# LED thread data\n");
        fprintf(f, "gpio_index,write_ns,jitter_ns\n");
        for (size_t i = 0; i < stats->gpio_samples; i++) {
            fprintf(f, "%zu,%ld,%ld\n",
                    i,
                    stats->gpio_write_ns[i],
                    stats->gpio_jitter_ns[i]);
        }
    }

    fclose(f);
    printf("Playback report saved to: %s\n", filename);
}

// Legacy function for compatibility
void save_runtime_log(const char *filename,
                      long *runtimes_us,
                      long *wake_intervals_us,
                      long *jitter_us,
                      size_t runtime_index,
                      int underrun_count) {
    FILE *f = fopen(filename, "w");
    if (!f) { perror("runtime log fopen"); return; }

    fprintf(f, "index,runtime_us,wake_interval_us,jitter_us\n");
    long sum = 0, max = 0;
    for (size_t i = 0; i < runtime_index; ++i) {
        fprintf(f, "%zu,%ld,%ld,%ld\n", i, runtimes_us[i],
                wake_intervals_us[i], jitter_us[i]);
        sum += runtimes_us[i];
        if (runtimes_us[i] > max) max = runtimes_us[i];
    }

    double avg = (runtime_index > 0) ? (double)sum / runtime_index : 0;
    fprintf(f, "\nAverage (us),%lf\nMax (us),%ld\n", avg, max);
    fprintf(f, "Total underruns,%d\n", underrun_count);
    fclose(f);
}
