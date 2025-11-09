CC = gcc
CFLAGS = -Wall -O2 -pthread
LDFLAGS = -lasound
INCLUDE = -Iinclude

SRC = src/main.c \
      src/player.c \
      src/gpio.c \
      src/udp.c \
      src/setup_alsa.c \
      src/load.c \
      src/log.c

all: sequencer

sequencer: $(SRC)
	$(CC) $(SRC) $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@

clean:
	rm -f sequencer
