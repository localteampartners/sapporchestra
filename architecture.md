# SappOrchestra Architecture

**Status:** implemented, v0.1.0
**Products:** Standalone app, VST3, AU (JUCE 8), plus an agent CLI
**Foundation:** SappOrchestra is a product built on **SappSounds**, the
framework-independent sample-instrument engine in a sibling repository.

> The original build-ready design lived in `SappOrchestra_architecture.md`
> (pre-split). This document supersedes it: the generic sampler layer was
> extracted into SappSounds *before* implementation, so there is no
> `SappAudio` parent project and no `SappSampleCore`. See
> [docs/boundary_report.md](docs/boundary_report.md) for the exact split.

## 1. Dependency diagram

```text
┌────────────────────────────────────────────────────────┐
│ SappOrchestra (this repo)                              │
│                                                        │
│  src/plugin/   JUCE processor + editor (UI, params,    │
│                state, async loading, MIDI conversion)  │
│  src/cli/      sapporchestra agent CLI (JSON)          │
│  src/core/     OrchestraEngine · Reverb · Render       │
│                (framework-free product policy)         │
└───────────────────────┬────────────────────────────────┘
                        │ target_link_libraries(... Sapp::Sounds)
                        ▼
┌────────────────────────────────────────────────────────┐
│ SappSounds (../sappsounds)                             │
│  SFZ parser · instrument loader · sample decode ·      │
│  region selection · voices · RR/keyswitch/release ·    │
│  loops · resampling · stealing · diagnostics           │
└────────────────────────────────────────────────────────┘
```

SappSounds never references SappOrchestra. Integration is
`add_subdirectory(../sappsounds)` for local dev, with FetchContent fallback
(see CMakeLists.txt).

## 2. What this repo owns

| Layer | Files | Responsibility |
|---|---|---|
| Orchestra policy | `src/core/OrchestraEngine.{h,cpp}` | CC1 dynamics (level+timbre LP), CC11 expression, stage placement (pan/width/depth → damping/predelay/sends), Analog DNA (per-note detune via the SappSounds hook, drift, vintage noise), quality modes, soft-limiter output policy, UI-driven articulation switching via keyswitch injection |
| Room | `src/core/Reverb.h` | Early reflections (8 asymmetric taps, predelay, absorption) + shared hall (8-line FDN, Householder feedback, per-line damping, gentle modulation) |
| Offline | `src/core/OrchestraRender.{h,cpp}` | Deterministic MIDI → full-chain render |
| Plugin | `src/plugin/` | APVTS parameters (IDs are contracts, version 1), knob→CC bridging, MIDI conversion, async instrument loading with generation guards, host state (schema v1: APVTS + `sfzPath`), editor |
| UI | `src/plugin/PluginEditor.*` | "Concert hall at night" LookAndFeel, articulation chips, stage pad, keyswitch-aware keyboard, X-Ray-fed voice/meter display |
| Agent API | `src/cli/main.cpp` | `inspect / validate / params / render` JSON commands |

## 3. Signal flow (audio thread)

```text
Host MIDI + injected events (knob→CC, UI articulation keyswitch)
   ▼
sapp::sounds::PlaybackEngine        (dry, additive, realtime-safe)
   ▼
width (M/S) → dynamics timbre LP → distance LP → dynamics+expression gain
   → DNA drift/noise → stage pan
   ├─ direct (depth-attenuated) ────────────────────────┐
   ├─ early reflections (predelay, skew, absorption) ───┤ mix
   └─ hall FDN (fed by direct + ER for coherence) ──────┘
   ▼
master gain → optional soft limiter (tanh) → out
```

All parameter changes reach the audio thread through a double-buffered
`OrchestraParams` slot (atomic index flip); per-sample smoothing avoids zipper
noise. CC1/CC11 persist as live overrides so ridden phrases survive across
blocks; moving the Dynamics/Expression knobs injects the matching CC so both
paths stay in sync.

## 4. Articulations

`ArticulationInfo` facts come from SappSounds (derived from `sw_last`
groups). This repo turns them into policy: UI chips, an automatable
`articulation` parameter (0–15), and `selectArticulation()` which injects the
keyswitch note at the head of the next block — so host automation, UI clicks,
and plain keyswitch playing all converge on the same engine state, and the
X-Ray snapshot's `activeKeyswitch` lights the correct chip/key.

## 5. Parameters (IDs are contracts — never reuse)

`dynamics expression stageX stageDepth width earlyLevel tailLevel hallSize
hallDecay hallDamping dnaMode dnaAmount masterGain limiter quality
articulation` — all version 1. State schema v1 = APVTS tree + `sfzPath`
property. Missing SFZ at restore falls back to the Diagnostic Orchestra with
status text (never silently substitutes another instrument… the diagnostic
fallback is explicit in the UI).

## 6. Testing

- `tests/unit/test_orchestra_engine.cpp` — dynamics level+brightness,
  expression, stage pan/depth, articulation injection, limiter/NaN guards
- `tests/unit/test_room.cpp` — ER predelay/decay, FDN density/decay/finite,
  T60 scaling
- `tests/unit/test_render.cpp` — deterministic full-chain renders,
  articulation-index renders
- SappSounds' own 45-case suite runs independently (`SappSoundsTests`)

## 7. Roadmap pointers

Streaming (in SappSounds), Phase-2 SFZ opcodes, library browser/indexer UI,
section buses + multi-instrument rack, convolution hall option, MPE, X-Ray
panel in the editor (data feed already exists), pluginval/auval in CI.
See `_project/ROADMAP.md`.
