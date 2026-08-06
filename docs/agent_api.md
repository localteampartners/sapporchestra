# SappOrchestra Agent API

The `sapporchestra` CLI is the stable machine interface for external
software — MIDI-generation agents in particular. Every command prints exactly
one JSON document to stdout; diagnostics go to stderr; exit codes are
`0` ok, `1` ok-with-warnings, `2` failure.

Binary: `build/sapporchestra` (CMake target `sapporchestra-cli`).

## Typical agent workflow

```text
1. sapporchestra inspect  → learn range, articulations, keyswitch protocol
2. compose MIDI           → keyswitches + CC1 phrasing + CC11 shaping
3. sapporchestra render   → deterministic WAV (fixed --seed)
4. judge / iterate
```

## inspect

```bash
sapporchestra inspect (--sfz <file.sfz> | --diagnostic) [--regions]
```

```json
{
  "ok": true,
  "name": "Diagnostic Orchestra",
  "regions": 110,
  "missingSamples": 0,
  "estimatedRamBytes": 43084096,
  "playableRange": {"low": 21, "high": 99, "lowName": "A0", "highName": "D#7"},
  "articulations": [
    {"index": 0, "name": "Sustain", "keyswitch": 12, "keyswitchName": "C0",
     "regions": 22, "default": true}
  ],
  "capabilities": {"velocityLayers": 2, "roundRobins": 2,
                    "releaseSamples": false, "keyswitches": true},
  "controllers": [
    {"cc": 1, "role": "dynamics", "doc": "..."},
    {"cc": 11, "role": "expression", "doc": "..."},
    {"cc": 64, "role": "sustain", "doc": "..."}
  ],
  "diagnostics": []
}
```

Composition rules an agent should follow:

- Keep notes inside `playableRange`.
- Switch articulations by emitting the articulation's `keyswitch` note
  (short, any velocity) *before* the musical notes — or pass
  `--articulation <index>` to fix one for a whole render.
- Ride CC1 through phrases (swells breathe); use CC11 for phrase-level
  balance; CC64 is a real sustain pedal (deferred releases).
- `--regions` adds a per-region dump for mapping-level analysis.

## validate

```bash
sapporchestra validate --sfz <file.sfz>
```

`{"ok":bool, "errors":N, "warnings":N, "missingSamples":N, "regions":N,
"unsupportedOpcodes":[...], "diagnostics":[{severity,file,line,message}]}`

## params

```bash
sapporchestra params
```

Returns the full parameter schema: `{"params":[{name,min,max,default,doc}],
"enums":{"dna_mode":[...],"quality":[...]}}`. Use these names with
`render --param`.

| name | range | default | meaning |
|---|---|---|---|
| dynamics | 0–1 | 0.7 | CC1 equivalent: level + timbre |
| expression | 0–1 | 1.0 | CC11 equivalent: phrase volume |
| stage_x | −1–1 | 0 | stage position left→right |
| stage_depth | 0–1 | 0.35 | close→far (attenuation, damping, room) |
| width | 0–2 | 1 | stereo width before positioning |
| early_level | 0–1 | 0.35 | early-reflection level |
| tail_level | 0–1 | 0.30 | hall tail level |
| hall_size | 0.2–1.5 | 1.0 | hall size |
| hall_decay | 0.3–12 | 2.6 | T60 seconds |
| hall_damping | 0–1 | 0.45 | HF damping |
| dna_amount | 0–1 | 0.18 | ensemble detune/drift amount |
| master_gain_db | −24–12 | 0 | output gain |
| dna_mode | enum | 1 | 0 clean · 1 cohesive · 2 vintage |
| quality | enum | 1 | 0 draft (linear) · 1 normal (cubic) |

## render

```bash
sapporchestra render (--sfz <file.sfz> | --diagnostic) \
    --midi <file.mid> --out <file.wav> \
    [--sr 48000] [--seed N] [--tail seconds] \
    [--articulation INDEX] [--param NAME=VALUE ...]
```

- Input: SMF format 0/1. Notes, CCs (1/11/64/...), pitch bend are honored;
  keyswitch notes in the MIDI stream switch articulations mid-piece.
- Output: stereo float32 WAV through the full chain (sampler → dynamics →
  stage → early reflections → shared hall → limiter).
- **Deterministic:** identical inputs + `--seed` ⇒ bit-identical WAV. Vary
  the seed for new round-robin/humanization takes.

Result: `{"ok":true, "out":..., "frames":N, "durationSeconds":s,
"peak":p, "rms":r, "midiEvents":N, "seed":N}`.

## Multi-instrument arrangements

Render one stem per instrument/section (different `--sfz`, `stage_x`,
`stage_depth`, shared `hall_*` values), then mix stems; keeping hall
parameters identical preserves the one-room illusion.
[scripts/make_demo.py](../scripts/make_demo.py) is a working example
(3 articulation stems → placed → mixed → normalized).

## Lower-level engine tools

The SappSounds repo ships engine-level equivalents without the orchestra
chain: `SappSoundsSFZValidator`, `SappSoundsInstrumentInspector`,
`SappSoundsRenderTool` (dry renders). Prefer the `sapporchestra` CLI for
musical output.

## Stability

Command names, field names, exit codes, and parameter names are contracts.
New fields may be added; existing ones are not renamed or repurposed.
