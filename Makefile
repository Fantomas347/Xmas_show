CC = gcc
CFLAGS = -Wall -O2 -pthread
LDFLAGS = -lasound -lmpg123
INCLUDE = -Iinclude

# Platform: RPI1, RPI2, RPI3, RPI4 (default: RPI1)
PLATFORM ?= RPI4

# Enable detailed trace logging to file (disabled by default)
# Use: make ENABLE_TRACE=1 to enable full CSV logging
ENABLE_TRACE ?= 0

SRC = src/main.c \
      src/player.c \
      src/gpio.c \
      src/udp.c \
      src/setup_alsa.c \
      src/load.c \
      src/audio.c \
      src/log.c

all: sequencer

DEFINES = -D$(PLATFORM)
ifeq ($(ENABLE_TRACE),1)
	DEFINES += -DENABLE_TRACE
endif

sequencer: $(SRC)
	$(CC) $(SRC) $(INCLUDE) $(CFLAGS) $(DEFINES) $(LDFLAGS) -o $@

# Set capabilities for GPIO and real-time scheduling (run after build)
setcap:
	sudo setcap cap_sys_rawio,cap_sys_nice+ep sequencer

clean:
	rm -f sequencer

# Install libmpg123 on Raspberry Pi:
#   sudo apt-get install libmpg123-dev
