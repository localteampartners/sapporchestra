#!/usr/bin/env python3
"""Compose a short orchestral demo for SappOrchestra and render it as stems."""
import json
import struct
import subprocess
import sys
import wave

CLI = sys.argv[1] if len(sys.argv) > 1 else "./build/sapporchestra"
OUT = sys.argv[2] if len(sys.argv) > 2 else "/tmp/sapporchestra-demo.wav"
TPQ = 480
BPM = 84  # tempo via microseconds per quarter


def write_midi(path, events, tempo_bpm=BPM):
    events = sorted(events, key=lambda e: e[0])
    track = b""
    last = 0
    # tempo meta first
    us = int(60_000_000 / tempo_bpm)
    track += bytes([0x00, 0xFF, 0x51, 0x03]) + us.to_bytes(3, "big")
    for tick, data in events:
        delta = tick - last
        last = tick
        vlq = [delta & 0x7F]
        d = delta >> 7
        while d:
            vlq.append(0x80 | (d & 0x7F))
            d >>= 7
        track += bytes(reversed(vlq)) + bytes(data)
    track += bytes([0x00, 0xFF, 0x2F, 0x00])
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPQ)
    with open(path, "wb") as f:
        f.write(header + b"MTrk" + struct.pack(">I", len(track)) + track)


def beats(b):
    return int(b * TPQ)


def note(events, start_b, dur_b, key, vel):
    events.append((beats(start_b), [0x90, key, vel]))
    events.append((beats(start_b + dur_b), [0x80, key, 0]))


def cc(events, at_b, num, val):
    events.append((beats(at_b), [0xB0, num, val]))


def ramp(events, num, start_b, end_b, v0, v1, steps=16):
    for i in range(steps + 1):
        t = start_b + (end_b - start_b) * i / steps
        v = int(v0 + (v1 - v0) * i / steps)
        cc(events, t, num, max(0, min(127, v)))


# ---- Musical material: i – VI – III – VII in A minor, 16 beats ---------------
CHORDS = [
    [45, 52, 57, 60, 64],   # Am  (A2 E3 A3 C4 E4)
    [41, 48, 53, 57, 60],   # F   (F2 C3 F3 A3 C4)
    [48, 55, 60, 64, 67],   # C   (C3 G3 C4 E4 G4)
    [43, 50, 55, 59, 62],   # G   (G2 D3 G3 B3 D4)
]
BASS = [33, 29, 36, 31]     # A1 F1 C2 G1

# --- Stem 1: sustained strings, breathing CC1 swells, centre-left ------------
sus = []
cc(sus, 0, 1, 30)
note(sus, 0, 0.1, 12, 100)          # keyswitch: Sustain
for bar, chord in enumerate(CHORDS):
    t = bar * 4
    for k in chord:
        note(sus, t + 0.02, 3.9, k, 76)
    ramp(sus, 1, t, t + 2.0, 34, 96)      # swell up
    ramp(sus, 1, t + 2.0, t + 3.9, 96, 40)  # relax
# final chord, big swell and release
t = 16
for k in CHORDS[0]:
    note(sus, t, 6.0, k, 88)
note(sus, t, 6.0, 69, 82)
ramp(sus, 1, t, t + 3.0, 40, 112)
ramp(sus, 1, t + 3.0, t + 6.0, 112, 20)

# --- Stem 2: staccato answers, right side ------------------------------------
stac = []
note(stac, 0, 0.1, 13, 100)         # keyswitch: Staccato
cc(stac, 0, 1, 80)
PATTERN = [(2.0, 0), (2.5, 1), (3.0, 2), (3.5, 1)]
for bar, chord in enumerate(CHORDS):
    t = bar * 4
    if bar % 2 == 0:
        for off, deg in PATTERN:
            note(stac, t + off, 0.22, chord[2 + deg % 3] + 12, 96 + (8 if off == 2.0 else 0))
    else:
        for i, off in enumerate([2.0, 2.25, 2.5, 2.75, 3.0, 3.5]):
            note(stac, t + off, 0.18, chord[(i * 2) % len(chord)] + 12, 88 + 6 * (i % 2))
# closing figure
for i, off in enumerate([16.0, 16.5, 17.0]):
    note(stac, off, 0.22, [76, 72, 69][i], 100 - i * 8)

# --- Stem 3: pizzicato bass pulse, left --------------------------------------
pizz = []
note(pizz, 0, 0.1, 14, 100)         # keyswitch: Pizzicato
cc(pizz, 0, 1, 70)
for bar, root in enumerate(BASS):
    t = bar * 4
    for off, v in [(0, 104), (1.5, 84), (2.0, 96), (3.0, 88)]:
        note(pizz, t + off, 0.4, root + 12, v)  # +12: keep in sampled range
note(pizz, 16, 1.2, 45, 104)

STEMS = [
    ("sustain", sus, {"stage_x": -0.25, "stage_depth": 0.45, "width": 1.25,
                      "tail_level": 0.34, "early_level": 0.32, "dna_amount": 0.35}),
    ("staccato", stac, {"stage_x": 0.45, "stage_depth": 0.3, "width": 0.9,
                        "tail_level": 0.26, "early_level": 0.38, "dna_amount": 0.25}),
    ("pizz", pizz, {"stage_x": -0.55, "stage_depth": 0.25, "width": 0.7,
                    "tail_level": 0.2, "early_level": 0.35, "dna_amount": 0.2}),
]

mixed = None
sr = 48000
for name, events, params in STEMS:
    mid = f"/tmp/demo-{name}.mid"
    wav = f"/tmp/demo-{name}.wav"
    write_midi(mid, events)
    cmd = [CLI, "render", "--diagnostic", "--midi", mid, "--out", wav,
           "--tail", "5", "--seed", "42",
           "--param", "hall_decay=3.4", "--param", "hall_size=1.1"]
    for k, v in params.items():
        cmd += ["--param", f"{k}={v}"]
    result = json.loads(subprocess.check_output(cmd).decode())
    print(name, "->", result["durationSeconds"], "s, peak", round(result["peak"], 3))
    blob = open(wav, "rb").read()
    # SappSounds writer: RIFF/fmt(16)/data, float32 stereo interleaved.
    assert blob[:4] == b"RIFF" and blob[36:40] == b"data"
    raw = blob[44:]
    samples = list(struct.unpack(f"<{len(raw)//4}f", raw))
    if mixed is None:
        mixed = samples
    else:
        if len(samples) > len(mixed):
            mixed += [0.0] * (len(samples) - len(mixed))
        for i, v in enumerate(samples):
            mixed[i] += v

# Normalize to -1 dBFS and write final mix (16-bit for easy playback).
peak = max(abs(v) for v in mixed)
gain = (10 ** (-1 / 20)) / peak if peak > 0 else 1.0
pcm = struct.pack(f"<{len(mixed)}h",
                  *[max(-32768, min(32767, int(v * gain * 32767))) for v in mixed])
with wave.open(OUT, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(sr)
    w.writeframes(pcm)
print("mix ->", OUT, f"({len(mixed)//2/sr:.1f} s, peak normalized to -1 dBFS)")
