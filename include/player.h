#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

void play_song(const char *base_name);
void reset_runtime_state(void);
void set_verbose_mode(int enabled);
void set_music_dir(const char *dir);

#endif
