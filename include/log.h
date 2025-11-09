#ifndef LOG_H
#define LOG_H

#include <stddef.h>

void save_runtime_log(const char *filename,
		      long *runtimes_us,
		      long *wake_intervals_us,
		      long *jitter_us,
		      size_t runtime_index,
		      int underrun_count);

#endif

