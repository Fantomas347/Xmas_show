# Real-Time LED + Music Sequencer

A hard real-time LED and audio sequencer for Raspberry Pi. Synchronizes audio playback (MP3/WAV) with precisely timed LED patterns via GPIO.

## Features

- Real-time operation on Raspberry Pi 1/2/3/4
- Supports MP3 and WAV audio formats
- Dynamic sample rate handling (32kHz, 44.1kHz, 48kHz)
- Direct GPIO register access (memory-mapped)
- Multi-threaded design with SCHED_FIFO real-time scheduling
- LED thread: 10ms period, priority 80
- Audio thread: 30ms period, priority 75
- WAV files: mmap + mlock for hard real-time (no disk I/O during playback)
- MP3 files: ring buffer with ~3 sec pre-buffer for soft real-time
- Graceful shutdown with immediate LED turn-off on SIGTERM/SIGINT
- Optional UDP control mode
- Timing and jitter logging

## Dependencies

```bash
sudo apt-get install libasound2-dev libmpg123-dev
```

## Building

```bash
# For Raspberry Pi 4
make PLATFORM=RPI4

# For Raspberry Pi 1/Zero
make PLATFORM=RPI1

# Set capabilities for non-root execution
make setcap
```

Available platforms: `RPI1`, `RPI2`, `RPI3`, `RPI4`

## Capabilities Setup

The sequencer needs elevated privileges for GPIO access and real-time scheduling. Instead of running as root, use Linux capabilities:

```bash
sudo setcap cap_sys_rawio,cap_sys_nice+ep ./sequencer
```

Or use the Makefile target:
```bash
make setcap
```

This grants:
- `cap_sys_rawio`: GPIO memory mapping
- `cap_sys_nice`: SCHED_FIFO real-time scheduling

Note: Capabilities must be re-applied after each recompile.

## Usage

```bash
# Play a song (loads songname.mp3/.wav and songname.txt from music dir)
./sequencer songname

# Specify custom music directory
./sequencer -m /path/to/music/ songname

# Verbose mode (print timing stats)
./sequencer -v songname

# Turn all LEDs on and exit
./sequencer -s on

# Turn all LEDs off and exit
./sequencer -s off

# Interactive menu mode
./sequencer
```

## Directory Structure

```
/src        - C source files
/include    - Header files
/test       - Test files
```

## LED Pattern Format

Pattern files (`.txt`) must match the audio filename. Each line:

```
TTTT BBBB.BBBB
```

- `TTTT`: Duration in milliseconds (minimum 10ms)
- `BBBB.BBBB`: 8-bit LED pattern (1=on, 0=off), dot is optional separator

Example:
```
0100 1010.1100
0050 0101.0011
0200 1111.1111
```

## Hardware Requirements

- Raspberry Pi (1/2/3/4)
- 8 LEDs connected to GPIO pins
- Audio output (3.5mm jack or HDMI)
- ALSA-compatible audio

## Signal Handling

The sequencer handles SIGTERM and SIGINT for graceful shutdown:

1. Signal handler immediately turns off all LEDs via GPIO
2. Sets `stop_requested` flag for threads to check
3. Threads exit their loops after current sleep cycle
4. Main thread waits for threads to join
5. Final GPIO cleanup and exit

This ensures LEDs are turned off immediately when playback is stopped externally (e.g., via the v43 controller).

## Integration with v43-christmas-lights

This sequencer is designed to work with the v43-christmas-lights Node.js controller. The controller spawns the sequencer as a child process for each song playback and sends SIGTERM to stop playback.

See `/home/linux/v43-christmas-lights/` for the controller setup.

---

## Technical Reference: How It Works

### Architecture Overview

```
    [Main Thread]
         |
         | spawns threads
         |
    +----+--------+--------------+
    |             |              |
    v             v              v
+--------+   +--------+   +------------+
|  LED   |   | Audio  |   |  Decoder   | (MP3 only)
| Thread |   | Thread |   |   Thread   |
+--------+   +--------+   +------------+
    |             |              |
    | reads       | reads        | writes
    v             v              v
+--------+   +---------------------+
|Pattern |   | mmap'd WAV         |
| Array  |   |        or          |
+--------+   | Ring Buffer (MP3)  |
    |        +---------------------+
    |             |
    | writes      | writes
    v             v
[GPIO pins]  [ALSA/Speaker]
```

### Real-Time Scheduling (SCHED_FIFO)

The program uses Linux's SCHED_FIFO (First-In-First-Out) real-time scheduling policy:

- **LED thread**: Priority 80 (highest) - ensures LED timing is never interrupted
- **Audio thread**: Priority 75 - high priority but yields to LED thread
- **Decoder thread**: Normal priority - runs in background filling buffer

SCHED_FIFO threads preempt all normal processes and run until they voluntarily yield via `clock_nanosleep()`.

Both real-time threads use **absolute time sleeps** to prevent timing drift:

```c
clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
// do work...
next_time.tv_nsec += PERIOD_MS * 1000000;  // schedule next wake
```

### Audio Playback

**WAV files (hard real-time):**
1. File is `mmap()`'d into memory at startup
2. `mlock()` pins it to RAM - prevents page faults during playback
3. Audio thread reads directly via `memcpy()` - no disk I/O during playback

**MP3 files (soft real-time):**
1. Decoder thread calls `mpg123_read()` to decode MP3 → PCM
2. Decoded samples written to ring buffer (~3 seconds capacity)
3. Audio thread reads from ring buffer
4. 100ms pre-buffer before playback starts

**ALSA configuration:**
- Period size: ~10ms of audio (e.g., 480 frames at 48kHz)
- Buffer size: ~120ms (12 periods) - provides tolerance for scheduling jitter
- Audio thread writes samples via `snd_pcm_writei()`
- Hardware consumes buffer via DMA at constant rate

### GPIO Control

The program uses memory-mapped GPIO for minimal latency:

1. Opens `/dev/gpiomem` and maps GPIO registers into userspace
2. Writes directly to GPSET0/GPCLR0 registers (no syscalls)
3. `__sync_synchronize()` memory barrier ensures write ordering

### RT_PREEMPT Kernel

For best real-time performance, use an RT_PREEMPT kernel. This makes the Linux kernel fully preemptible.

| Kernel Type | Typical Jitter | Worst-Case Jitter |
|-------------|----------------|-------------------|
| Standard    | 10-100 µs      | 1-10 ms (spikes)  |
| RT_PREEMPT  | 10-50 µs       | 50-150 µs         |

**Benefits of RT_PREEMPT:**
- Hardware interrupt handlers become preemptible kernel threads
- Your SCHED_FIFO threads can preempt most interrupt handlers
- Full priority inheritance prevents priority inversion
- Consistent timing even under system load

**When you need RT_PREEMPT:**
- Running other services alongside the sequencer
- Need guaranteed timing for professional light shows
- Verbose mode (`-v`) shows frequent deadline misses

**Standard kernel is usually fine if:**
- Raspberry Pi is dedicated to this program
- Low system load
- Occasional small glitches are acceptable

### Installing RT Kernel on Raspberry Pi OS (kernel v6.12+)

```bash
# Install the RT kernel package
sudo apt install linux-image-rpi-v8-rt

# Edit boot configuration
sudo vim /boot/firmware/config.txt

# Add this line near the top (after initial comments):
kernel=kernel8_rt.img

# Reboot to use the RT kernel
sudo reboot
```

**Verify RT kernel is active:**
```bash
uname -a
# Should show "PREEMPT_RT" in the output

cat /sys/kernel/realtime
# Returns "1" if RT_PREEMPT is active
```
