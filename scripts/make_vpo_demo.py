#!/usr/bin/env python3
"""Render a short orchestral piece with Virtual Playing Orchestra through
the sapporchestra CLI: real sections, seated on the stage, one shared hall.

Usage: make_sonatina_demo.py [cli-path] [sonatina-root] [out.wav]
"""
import json
import os
import struct
import subprocess
import sys

CLI = sys.argv[1] if len(sys.argv) > 1 else "./build/sapporchestra"
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser(
    "~/Samples/vpo/Virtual-Playing-Orchestra3")
OUT = sys.argv[3] if len(sys.argv) > 3 else "/tmp/sapporchestra-vpo-demo.wav"
TPQ = 480
BPM = 76


def write_midi(path, events):
    events = sorted(events, key=lambda e: e[0])
    track = b""
    us = int(60_000_000 / BPM)
    track += bytes([0x00, 0xFF, 0x51, 0x03]) + us.to_bytes(3, "big")
    last = 0
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
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPQ))
        f.write(b"MTrk" + struct.pack(">I", len(track)) + track)


def b(beats):
    return int(beats * TPQ)


def note(ev, start, dur, key, vel):
    ev.append((b(start), [0x90, key, vel]))
    ev.append((b(start + dur), [0x80, key, 0]))


def cc(ev, at, num, val):
    ev.append((b(at), [0xB0, num, max(0, min(127, int(val)))]))


def ramp(ev, num, t0, t1, v0, v1, steps=20):
    for i in range(steps + 1):
        cc(ev, t0 + (t1 - t0) * i / steps, num, v0 + (v1 - v0) * i / steps)


# --- The piece: 8 bars in D minor, ~25 s -------------------------------------
# i (Dm) – VI (Bb) – III (F) – VII (C)  x2, closing on Dm.
CHORDS = [
    [50, 57, 62, 65],   # Dm:  D3 A3 D4 F4
    [46, 53, 58, 62],   # Bb:  Bb2 F3 Bb3 D4
    [41, 48, 53, 57],   # F:   F2 C3 F3 A3
    [48, 55, 60, 64],   # C:   C3 G3 C4 E4
]
BASS = [38, 34, 41, 36]          # D2 Bb1 F2 C2
MELODY = [                        # (start-beat, dur, note, vel) violin line
    (8.0, 2.0, 74, 80), (10.0, 1.0, 77, 84), (11.0, 1.0, 76, 78),
    (12.0, 2.0, 74, 82), (14.0, 1.5, 70, 76), (15.5, 0.5, 72, 70),
    (16.0, 3.0, 69, 84), (19.0, 1.0, 70, 72),
    (20.0, 2.0, 72, 86), (22.0, 2.0, 74, 88),
    (24.0, 6.0, 74, 92),
]

stems = []

# 1st violins: melody on sustain — slight overlaps make the lines slur
# (legato level 2 suppresses attacks and fades the previous note).
v1 = []
cc(v1, 0, 1, 70)
for start, dur, key, vel in MELODY:
    note(v1, start, dur * 1.06, key, vel)
ramp(v1, 1, 8, 12, 60, 95)
ramp(v1, 1, 16, 20, 70, 100)
ramp(v1, 1, 24, 28, 100, 118)
ramp(v1, 1, 28, 30, 118, 30)
stems.append(("1st Violins KS", "Strings/1st-violin-SEC-sustain.sfz", "violin1", v1, 1.0))

# 2nd violins + violas: chord pad, sustained, breathing with the harmony.
pad = []
cc(pad, 0, 1, 55)
for rep in range(2):
    for bar, chord in enumerate(CHORDS):
        t = rep * 16 + bar * 4
        for k in chord[1:]:
            note(pad, t + 0.03, 3.92, k, 66)
        ramp(pad, 1, t, t + 2, 42, 78)
        ramp(pad, 1, t + 2, t + 3.9, 78, 48)
for k in [50, 57, 62, 65]:
    note(pad, 32, 5.0, k, 72)
ramp(pad, 1, 32, 34.5, 55, 96)
ramp(pad, 1, 34.5, 37, 96, 20)
stems.append(("2nd Violins", "Strings/2nd-violin-SEC-sustain.sfz", "violin2", pad, 0.85))

violas = []
cc(violas, 1, 1, 55)
for rep in range(2):
    for bar, chord in enumerate(CHORDS):
        t = rep * 16 + bar * 4
        note(violas, t + 0.02, 3.9, chord[0] + 12, 62)
        note(violas, t + 0.02, 3.9, chord[1], 62)
note(violas, 32, 5.0, 62, 66)
note(violas, 32, 5.0, 57, 66)
stems.append(("Violas", "Strings/viola-SEC-sustain.sfz", "viola", violas, 0.8))

# Celli: warm sustained roots.
celli = []
cc(celli, 0, 1, 62)
for rep in range(2):
    for bar, chord in enumerate(CHORDS):
        t = rep * 16 + bar * 4
        note(celli, t, 3.95, chord[0], 72)
note(celli, 32, 5.5, 50, 78)
ramp(celli, 1, 32, 35, 62, 90)
stems.append(("Celli", "Strings/cello-SEC-sustain.sfz", "cello", celli, 0.95))

# Basses: pizzicato pulse.
basses = []
cc(basses, 0, 1, 70)
for rep in range(2):
    for bar, root in enumerate(BASS):
        t = rep * 16 + bar * 4
        for off, vel in [(0, 96), (2, 82), (3, 88)]:
            note(basses, t + off, 0.6, root + 12, vel)
note(basses, 32, 1.5, 50, 98)
stems.append(("Basses", "Strings/bass-SEC-pizzicato.sfz", "bass", basses, 0.9))

# Horns: enter halfway with a chorale swell.
horns = []
cc(horns, 0, 1, 40)
for bar, chord in enumerate(CHORDS):
    t = 16 + bar * 4
    note(horns, t + 0.05, 3.9, chord[1], 70)
    note(horns, t + 0.05, 3.9, chord[2], 70)
ramp(horns, 1, 16, 24, 30, 85)
note(horns, 32, 4.5, 57, 84)
note(horns, 32, 4.5, 62, 84)
ramp(horns, 1, 32, 34.5, 60, 105)
ramp(horns, 1, 34.5, 37, 105, 25)
stems.append(("Horns", "Brass/french-horn-SEC-sustain-DXF.sfz", "horn", horns, 0.85))

# Timpani: cadence accents.
timp = []
for t, vel in [(12.0, 90), (28.0, 96), (32.0, 118), (34.0, 70)]:
    note(timp, t, 1.0, 38, vel)
stems.append(("Timpani", "Percussion/timpani-hit.sfz", "timpani", timp, 1.0))

# --- Render each stem through the shared hall, then mix ----------------------
HALL = ["--param", "hall_decay=2.9", "--param", "hall_size=1.15",
        "--param", "hall_damping=0.5", "--param", "tail_level=0.34",
        "--param", "early_level=0.3", "--param", "dna_amount=0.3"]

sr = 48000
mixed = None
for name, rel, seat, events, gain in stems:
    sfz = os.path.join(LIB, rel)
    if not os.path.exists(sfz):
        print(f"SKIP {name}: {sfz} not found")
        continue
    mid = f"/tmp/vpo-{seat}.mid"
    wav = f"/tmp/vpo-{seat}.wav"
    write_midi(mid, events)
    cmd = [CLI, "render", "--sfz", sfz, "--midi", mid, "--out", wav,
           "--seat", seat, "--tail", "6", "--seed", "42"] + HALL
    result = json.loads(subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode())
    print(f"{name:14s} seat={seat:10s} {result['durationSeconds']:.1f}s peak {result['peak']:.3f}")
    blob = open(wav, "rb").read()
    samples = list(struct.unpack(f"<{(len(blob)-44)//4}f", blob[44:]))
    samples = [s * gain for s in samples]
    if mixed is None:
        mixed = samples
    else:
        if len(samples) > len(mixed):
            mixed += [0.0] * (len(samples) - len(mixed))
        for i, v in enumerate(samples):
            mixed[i] += v

peak = max(abs(v) for v in mixed)
g = (10 ** (-1 / 20)) / peak if peak > 0 else 1.0
pcm = struct.pack(f"<{len(mixed)}h",
                  *[max(-32768, min(32767, int(v * g * 32767))) for v in mixed])
import wave
with wave.open(OUT, "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(sr)
    w.writeframes(pcm)
print(f"mix -> {OUT} ({len(mixed)//2/sr:.1f}s, normalized to -1 dBFS)")
