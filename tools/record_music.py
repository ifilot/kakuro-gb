#!/usr/bin/env python3
"""Record the music as PyBoy plays it on the title screen to a WAV file.

  python3 tools/record_music.py build/kakuro.gb build/music.wav [seconds]
"""
from __future__ import annotations

import sys
import wave
from pathlib import Path

import numpy as np
from pyboy import PyBoy

FPS = 59.73                       # Game Boy frames a second


def main() -> int:
    rom, out = Path(sys.argv[1]), Path(sys.argv[2])
    seconds = float(sys.argv[3]) if len(sys.argv) > 3 else 75
    pb = PyBoy(str(rom), window="null", sound_emulated=True)
    pb.set_emulation_speed(0)
    chunks = []
    for _ in range(int(seconds * FPS)):
        pb.tick(1, False)
        chunks.append(np.array(pb.sound.ndarray, dtype=np.int16))
    rate = pb.sound.sample_rate
    pb.stop(save=False)
    samples = np.concatenate(chunks) * 256         # int8 -> int16
    with wave.open(str(out), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(samples.astype("<i2").tobytes())
    print(f"{out}: {len(samples) / rate:.0f} s at {rate} Hz")
    return 0


if __name__ == "__main__":
    sys.exit(main())
