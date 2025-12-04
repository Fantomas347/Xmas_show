#!/usr/bin/env python3
import wave
import math
from array import array
import itertools
import subprocess
import shutil

# --- CONFIGURABLE PARAMETERS ---
DURATION_SECONDS = 60          # total length of audio (2 minutes)
INTERVAL_MS = 400               # milliseconds between ticks (e.g. 0500)
TICK_DURATION_MS = 50           # length of each tick sound
SAMPLE_RATE = 44100             # Hz
TICK_FREQUENCY = 1000           # Hz for the tick tone
VOLUME = 1.0                    # 0.0 .. 1.0

AUDIO_WAV_FILENAME = "test.wav"
AUDIO_MP3_FILENAME = "test.mp3"
CONTROL_FILENAME = "test.txt"

# Patterns to write in the control file for each tick (cycled)
PATTERNS = [
    "0000.0001",
    "0000.0010",
    "0000.0100",
    "0000.1000",
    "0001.0000",
    "0010.0000",
    "0100.0000",
    "1000.0000",
]

#PATTERNS = [
#    "0000.0001",
#    "0000.0010",
#]

def generate_metronome_wav():
    num_samples = int(DURATION_SECONDS * SAMPLE_RATE)
    samples = array('h', [0] * num_samples)  # 16-bit signed samples

    interval_samples = int(SAMPLE_RATE * INTERVAL_MS / 1000.0)
    tick_len_samples = int(SAMPLE_RATE * TICK_DURATION_MS / 1000.0)

    # Create ticks at exact sample positions
    tick_positions = range(0, num_samples, interval_samples)

    for start in tick_positions:
        for i in range(tick_len_samples):
            idx = start + i
            if idx >= num_samples:
                break
            # Simple sine wave tick
            t = i / SAMPLE_RATE
            value = int(VOLUME * 32767 * math.sin(2 * math.pi * TICK_FREQUENCY * t))
            samples[idx] = value

    # Write WAV file (16-bit PCM, mono)
    with wave.open(AUDIO_WAV_FILENAME, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)  # 2 bytes = 16-bit
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(samples.tobytes())

    print(f"WAV file written: {AUDIO_WAV_FILENAME}")


def generate_control_file():
    interval_samples = int(SAMPLE_RATE * INTERVAL_MS / 1000.0)
    num_samples = int(DURATION_SECONDS * SAMPLE_RATE)
    num_ticks = num_samples // interval_samples

    pattern_cycle = itertools.cycle(PATTERNS)

    with open(CONTROL_FILENAME, "w", encoding="utf-8") as f:
        for _ in range(num_ticks):
            pattern = next(pattern_cycle)
            # First field: interval in ms, zero-padded to 4 digits (0500)
            f.write(f"{INTERVAL_MS:04d} {pattern}\n")

    print(f"Control file written: {CONTROL_FILENAME}")


def convert_wav_to_mp3():
    """
    Convert the generated WAV to MP3 using system ffmpeg.
    No external Python packages needed.
    """
    if shutil.which("ffmpeg") is None:
        print("ffmpeg not found in PATH – skipping MP3 conversion.")
        print("Install it with, for example: sudo apt install ffmpeg")
        return

    cmd = [
        "ffmpeg",
        "-y",               # overwrite output file
        "-loglevel", "error",
        "-i", AUDIO_WAV_FILENAME,
        "-codec:a", "libmp3lame",
        "-b:a", "192k",
        AUDIO_MP3_FILENAME,
    ]
    subprocess.run(cmd, check=True)
    print(f"MP3 file written: {AUDIO_MP3_FILENAME}")


if __name__ == "__main__":
    generate_metronome_wav()
    generate_control_file()
    convert_wav_to_mp3()

