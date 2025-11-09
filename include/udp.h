#ifndef UDP_H
#define UDP_H

#include <stddef.h>

#define MAX_SONG_NAME 64
#define UDP_PORT 5005

int receive_udp_song(char *song_out, size_t len);

#endif
