/**
 * V43 Christmas Lights Sequencer
 *
 * Real-time audio playback with synchronized LED control.
 * Supports MP3 (streaming) and WAV (mmap) formats.
 * Dynamic sample rate support (32kHz, 44.1kHz, 48kHz).
 *
 * Architecture:
 * =============
 *
 *                     +-------------------+
 *                     |  Decoder Thread   | (MP3 only, normal priority)
 *                     |  - mpg123_read()  |
 *                     |  - fills ring buf |
 *                     +---------+---------+
 *                               | writes
 *                               v
 *                     +-------------------+
 *                     |    Ring Buffer    | (~3 sec at 48kHz stereo)
 *                     +---------+---------+
 *                               | reads
 *                               v
 * +-------------------+   +-------------------+
 * |    LED Thread     |   |   Audio Thread    | (SCHED_FIFO, prio 75)
 * | SCHED_FIFO prio80 |   |  - audio_read()   |
 * | - 10ms tick rate  |   |  - ALSA writei()  |
 * | - GPIO mmap write |   |  - 30ms period    |
 * +-------------------+   +-------------------+
 *          |                       |
 *          v                       v
 *     [GPIO pins]            [ALSA/audio]
 *
 * Threading Model:
 * - LED thread:     SCHED_FIFO priority 80 (highest), 10ms period
 * - Audio thread:   SCHED_FIFO priority 75, 30ms period
 * - Decoder thread: Normal priority (MP3 only), runs ahead filling buffer
 *
 * For WAV files: mmap + mlock for hard real-time (no disk I/O during playback)
 * For MP3 files: Ring buffer with ~3 sec pre-buffer for soft real-time
 *
 * Capabilities required (non-root execution):
 * - cap_sys_rawio:  GPIO memory mapping
 * - cap_sys_nice:   SCHED_FIFO real-time scheduling
 */

#include "player.h"
#include "gpio.h"
#include "udp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>

#define MAX_SONG_NAME 64


void signal_handler(int sig)
{
    switch(sig)
    {
        case SIGINT:
        case SIGTERM:
            printf("Shutdown...\n");
            gpio_all_off(led_lines, 8);
            exit(EXIT_SUCCESS);
            break;

        //case SIGCHLD:
        //case SIGTSTP:
        case SIGTTOU:
        case SIGTTIN:
        // ignore it
        break;
        default:
            printf("Unhandled signal %s\n", strsignal(sig));
            break;
    }
}


void print_usage(const char *prog) {
    printf("Usage: %s [-v] [-m musicdir] [songname]\n", prog);
    printf("  -v              Verbose mode (print GPIO timing stats)\n");
    printf("  -m musicdir     Music directory (default: /home/linux/music/)\n");
    printf("  songname        Play song directly (without .wav/.txt extension)\n");
    printf("  No args         Interactive menu mode\n");
}

int main(int argc, char *argv[]) {

    openlog("sequencer", LOG_PID | LOG_CONS, LOG_USER);

    // Parse command line options
    int opt;
    while ((opt = getopt(argc, argv, "vm:h")) != -1) {
        switch (opt) {
            case 'v':
                set_verbose_mode(1);
                break;
            case 'm':
                set_music_dir(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    printf("Initializing GPIO...\n");
    gpio_init();
    gpio_set_outputs(led_lines, 8);
    gpio_all_off(led_lines, 8);

    // add signal handlers
    if (signal(SIGTTOU, signal_handler) == SIG_ERR) { exit(EXIT_FAILURE); }
    if (signal(SIGTTIN, signal_handler) == SIG_ERR) { exit(EXIT_FAILURE); }
    if (signal(SIGHUP,  signal_handler) == SIG_ERR) { exit(EXIT_FAILURE); }
    if (signal(SIGTERM, signal_handler) == SIG_ERR) { exit(EXIT_FAILURE); }
    if (signal(SIGINT,  signal_handler) == SIG_ERR) { exit(EXIT_FAILURE); }

    if (optind < argc) {
    // Parameter mode: just play the given song
    	play_song(argv[optind]);
    }
    else {
    // No parameter -> full menu mode

	    char choice[8];

	    while (1) {
		printf("\n=== LED + Music Sequencer ===\n");
		printf("1) Play song manually\n");
		printf("2) Receive song name via UDP JSON\n");
		printf("3) Exit\n> ");
		  printf("4) Emulate UDP from file\n");
		fflush(stdout);

		if (!fgets(choice, sizeof(choice), stdin))
		    break;
		int ch = atoi(choice);

		if (ch == 1) {
		    char base[MAX_SONG_NAME];
		    printf("Enter song base name (without .wav/.txt): ");
		    fflush(stdout);
		    if (!fgets(base, sizeof(base), stdin))
			continue;
		    base[strcspn(base, "\n")] = 0;
		    if (base[0] == '\0') {
			printf("Empty name, returning to menu.\n");
			continue;
		    }
		    play_song(base);

		} else if (ch == 2) {
		    char base[MAX_SONG_NAME];
		    if (receive_udp_song(base, sizeof(base)) == 0) {
			printf("UDP provided song: '%s'\n", base);
			printf("Play this song? (y/n): ");
			fflush(stdout);
			char ans[8];
			if (fgets(ans, sizeof(ans), stdin)) {
			    if (ans[0] == 'y' || ans[0] == 'Y')
				play_song(base);
			    else
				printf("Canceled, returning to menu.\n");
			}
		    } else {
			printf("No valid UDP song received (timeout or error).\n");
		    }

		} else if (ch == 3) {
		    printf("Exiting program.\n");
		    break;
		  } else if (ch == 4) {
			emulate_udp_from_file("udp_emulation.json");
		} else {
		    printf("Invalid choice. Try again.\n");
		}
	    }

    }

    gpio_cleanup();
    printf("GPIO cleaned up. Goodbye.\n");

    closelog();

    return 0;
}
