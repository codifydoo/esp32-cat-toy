#!/usr/bin/env python3
"""Synthesize UI sounds and placeholder animal sounds as 24 kHz mono 16-bit WAV (stdlib only)."""
import math
import random
import struct
import sys
import wave

RATE = 24000
random.seed(7)


def env(i, n, attack=0.01, release=0.3):
    t, dur = i / RATE, n / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    r = min(1.0, (dur - t) / (release * dur)) if release > 0 else 1.0
    return max(0.0, min(a, r))


def sweep(f0, f1, dur, harmonics=(1.0,), vibrato=0.0, attack=0.01, release=0.3):
    n = int(dur * RATE)
    out, ph = [], 0.0
    for i in range(n):
        f = f0 + (f1 - f0) * i / max(1, n - 1)
        f *= 1 + vibrato * math.sin(2 * math.pi * 6 * i / RATE)
        ph += 2 * math.pi * f / RATE
        v = sum(a * math.sin(k * ph) for k, a in enumerate(harmonics, 1))
        out.append(v * env(i, n, attack, release))
    return out


def glide(points, dur, **kw):
    seg = dur / (len(points) - 1)
    out = []
    for a, b in zip(points, points[1:]):
        out += sweep(a, b, seg, **kw)
    return out


def noise(dur, lp=0.2, attack=0.01, release=0.3, am=0.0):
    n = int(dur * RATE)
    out, y = [], 0.0
    for i in range(n):
        y += lp * (random.uniform(-1, 1) - y)
        m = 1.0 if am == 0 else 0.5 + 0.5 * math.sin(2 * math.pi * am * i / RATE)
        out.append(y * m * env(i, n, attack, release))
    return out


def silence(dur):
    return [0.0] * int(dur * RATE)


def mix(a, b):
    n = max(len(a), len(b))
    return [(a[i] if i < len(a) else 0.0) + (b[i] if i < len(b) else 0.0) for i in range(n)]


def notes(freqs, each, gap=0.0, **kw):
    out = []
    for f in freqs:
        out += sweep(f, f, each, **kw) + silence(gap)
    return out


def square(f, dur):
    n = int(dur * RATE)
    return [(0.5 if math.sin(2 * math.pi * f * i / RATE) >= 0 else -0.5) * env(i, n, 0.005, 0.1) for i in range(n)]


def bursts(count, each, gap, lp):
    out = []
    for _ in range(count):
        out += noise(each, lp=lp, attack=0.002, release=0.6) + silence(gap)
    return out


def saw(f, dur, attack, release):
    n = int(dur * RATE)
    return [((i * f / RATE) % 1.0 - 0.5) * env(i, n, attack, release) for i in range(n)]


C5, E5, G5, C6, E6, G6 = 523.25, 659.25, 783.99, 1046.5, 1318.5, 1568.0
BELL = (1.0, 0.0, 0.3, 0.0, 0.15)
PURR_AM = [0.6 + 0.4 * math.sin(2 * math.pi * 28 * i / RATE) for i in range(int(0.7 * RATE))]

RECIPES = {
    "blip_up": lambda: sweep(600, 1200, 0.12),
    "blip_down": lambda: sweep(1200, 600, 0.12),
    "pop": lambda: mix(sweep(420, 180, 0.08, release=0.9), noise(0.015, lp=0.9)),
    "click": lambda: noise(0.015, lp=0.9, attack=0.001, release=0.9),
    "chime_boot": lambda: notes([C6, E6, G6], 0.14, harmonics=BELL, release=0.8),
    "chime_wake": lambda: notes([C5, E5, G5, C6], 0.18, harmonics=BELL, release=0.8),
    "sparkle": lambda: sweep(3000, 3400, 0.04, harmonics=BELL, release=0.9),
    "bell_tick": lambda: sweep(2000, 2000, 0.06, harmonics=BELL, release=0.9),
    "pat": lambda: sweep(700, 500, 0.06, harmonics=(1, 0, 0.11, 0, 0.04), release=0.8),  # design: 700->500 Hz triangle
    "beep_boop": lambda: square(800, 0.12) + silence(0.04) + square(500, 0.14),
    "fanfare": lambda: notes([C5, E5, G5], 0.12, gap=0.02, harmonics=(1, 0.5, 0.25))
    + sweep(C6, C6, 0.5, harmonics=(1, 0.5, 0.25)),
    "meow": lambda: glide([520, 880, 640], 0.5, harmonics=(1, 0.6, 0.3, 0.15), vibrato=0.01, attack=0.05),
    "meow_high": lambda: glide([700, 1100, 800], 0.4, harmonics=(1, 0.6, 0.3), vibrato=0.01, attack=0.04),
    "meow_low": lambda: glide([380, 620, 450], 0.6, harmonics=(1, 0.6, 0.3, 0.15), vibrato=0.01, attack=0.06),
    "mew_down": lambda: glide([900, 500], 0.25, harmonics=(1, 0.5), attack=0.02),
    "mrrrow": lambda: [a * b for a, b in zip(glide([300, 520, 380], 0.7, harmonics=(1, 0.7, 0.4)), PURR_AM)],
    "purr": lambda: noise(1.5, lp=0.05, attack=0.1, release=0.2, am=25),
    "giggle": lambda: sum((sweep(900 + 80 * k, 1300 + 80 * k, 0.07, harmonics=(1, 0.4)) + silence(0.05) for k in range(5)), []),
    "wheee": lambda: sweep(400, 1500, 0.6, harmonics=(1, 0.4), vibrato=0.02),
    "boing": lambda: sweep(180, 420, 0.4, vibrato=0.25, release=0.7),
    "crunch": lambda: bursts(4, 0.04, 0.05, 0.6),
    "swish": lambda: noise(0.45, lp=0.35, attack=0.25, release=0.5),
    "rumble": lambda: noise(1.0, lp=0.03, attack=0.05, release=0.05),
    "whoosh": lambda: noise(0.3, lp=0.3, attack=0.5, release=0.5),
    "squeak": lambda: glide([2000, 3000, 2600], 0.12, harmonics=(1, 0.3)),
    "roar": lambda: mix(noise(0.6, lp=0.15, attack=0.1), saw(120, 0.6, 0.1, 0.4)),
    "yawn": lambda: glide([600, 700, 300], 0.8, harmonics=(1, 0.5, 0.2), attack=0.1),
    "munch": lambda: bursts(3, 0.05, 0.12, 0.4),
    "snore": lambda: noise(1.2, lp=0.04, attack=0.4, release=0.4, am=2),
}


def write(samples, path):
    peak = max(1e-9, max(abs(s) for s in samples))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(28000 * s / peak)) for s in samples))


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in RECIPES:
        sys.exit("usage: synth.py <%s> out.wav" % "|".join(sorted(RECIPES)))
    write(RECIPES[sys.argv[1]](), sys.argv[2])
