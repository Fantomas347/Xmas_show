Real-Time LED + Music Sequencer

A hard real-time LED and audio sequencer running on a Raspberry Pi 1
using low-level C. It synchronizes a WAV audio track with precisely
timed LED patterns, suitable for stage shows, visual effects, or
embedded demo purposes.

Overview

The sequencer plays: - a WAV audio file - a matching LED pattern file
(.txt) Both sharing the same base name.

Example: ./sequencer jungle

This loads: ~/music/jungle.wav ~/music/jungle.txt



Features

-   Real-time operation on Raspberry Pi 1
-   Direct GPIO register access
-   LED updates synchronized with audio
-   WAV playback using ALSA
-   Multi-threaded design
-   Optional UDP control mode
-   Timing and jitter logging
-   Works on low-power hardware



Directory Structure

/src → C source files /include → Header files /music → WAV + .txt
pattern pairs /logs → Timing logs



LED Pattern Format

Each line: [duration_ms] [8-bit LED pattern]

Example: 0100 1010.1100



Usage

Compile: make

Run: ./sequencer



Hardware Requirements

-   Raspberry Pi 1
-   8 LEDs connected to GPIO
-   Audio output through 3.5mm jack
-   ALSA library



Purpose

Demonstrates synchronized control of audio and LED subsystems using a
custom low-level engine.
