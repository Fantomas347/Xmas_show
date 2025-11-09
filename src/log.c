#include "log.h"
#include <stdio.h>
#include <stdlib.h>

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

    double avg = (double)sum / runtime_index;
    fprintf(f, "\nAverage (us),%lf\nMax (us),%ld\n", avg, max);
    fprintf(f, "Total underruns,%d\n", underrun_count);
    fclose(f);
}
