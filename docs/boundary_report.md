# SappSounds / SappOrchestra Boundary & Dependency Report

**Date:** 2026-08-06 · **Outcome:** clean two-repository split, applied at
design time.

## Context

The original `SappOrchestra_architecture.md` sketched a `SappAudio/`
umbrella tree with shared modules. Before any code was written, the design
was refactored: the generic sampler engine became **SappSounds**, a
standalone reusable library and repository, and SappOrchestra became a
product that links it. Because the split happened before implementation,
this is a *boundary* report rather than a file-migration report — no code
ever moved between repositories, none was duplicated, and there was no code
to rewrite.

## Responsibility split

### SappSounds owns (generic sampler)

| Concern | Implementation |
|---|---|
| SFZ parsing & validation | `src/SfzParser.cpp` (lexer, #include/#define, inheritance, diagnostics) |
| Immutable instrument definitions | `include/sapp/sounds/InstrumentDefinition.h` |
| Region definitions & lookup | per-note candidate tables in `src/PlaybackEngine.cpp` |
| Key/velocity mapping, keyswitches, CC conditions | region selection in `PlaybackEngine` |
| Round robin & random selection | per-note counters + seeded xorshift (deterministic) |
| Release triggers | `trigger=release/release_key` incl. pedal deferral |
| Loop playback | sustain/continuous loops, crossfade, smpl-chunk fallback |
| Sample metadata & decoding | `SampleData`, `src/WavIo.cpp` |
| Relative library-path handling | `src/InstrumentLoader.cpp` (default_path, case tolerance) |
| Sample caching / preloading | full-RAM preload v0.1; streaming interfaces specified in docs/streaming.md |
| Resampling | linear + Catmull-Rom cubic per-voice |
| Generic voice rendering & allocation | preallocated pool, priority stealing, de-click fades |
| Diagnostics | seqlock `DiagnosticSnapshot` (X-Ray feed) |

### SappOrchestra owns (orchestra product)

| Concern | Implementation |
|---|---|
| Orchestral articulation policy | chips/parameter/keyswitch-injection in core + plugin |
| CC1 dynamics behavior | level + timbre-LP mapping in `OrchestraEngine` |
| CC11 expression behavior | phrase-gain mapping in `OrchestraEngine` |
| Stage seating & placement | XY pad → pan/width/depth/damping/predelay |
| Early reflections & hall | `src/core/Reverb.h` (taps + 8-line FDN) |
| Analog DNA behavior | detune via SappSounds hook + drift/noise policy |
| Presets / host state | APVTS schema v1 + `sfzPath` |
| Orchestra UI | `src/plugin/PluginEditor.*` |
| Plugin wrappers & standalone | JUCE targets (VST3/AU/Standalone) |
| Agent CLI | `src/cli/main.cpp` |
| Solo/section, legato phrase behavior, section buses, guided experiments | roadmap items, land here |

## Dependency report

- Direction: `SappOrchestra → Sapp::Sounds` only. Verified: no header or
  source under `sappsounds/` includes or uses anything from
  `sapporchestra/` — a grep finds only two doc comments naming SappOrchestra
  as an example consumer — and SappSounds builds and its 45 tests pass from
  its own repo with SappOrchestra absent.
- SappSounds public API is framework-independent: no JUCE types anywhere in
  `include/sapp/sounds/`. JUCE appears only in SappOrchestra's plugin/UI
  targets.
- The one intentionally shared *tool* helper is the ~90-line write-only JSON
  writer (`tools/common/Json.h` ↔ `src/cli/Json.h`) — CLI plumbing, not
  engine code; no sampler implementation is duplicated.
- CMake: `add_subdirectory(${SAPPSOUNDS_DIR})` (sibling default) with a
  FetchContent fallback; consumers link the `Sapp::Sounds` alias. Submodule
  and installed-package modes are documented in sappsounds/docs/integration.md.

## Acceptance criteria status

| Criterion | Status |
|---|---|
| SappSounds builds without SappOrchestra | ✅ (own repo, own CI-able build) |
| SappSounds tests run without SappOrchestra | ✅ 45/45 |
| SappOrchestra links against SappSounds | ✅ `Sapp::Sounds` |
| Same supported SFZ instruments playable | ✅ single engine, no fork |
| No duplicated sampler implementation | ✅ (JSON tool helper only) |
| No SappSounds header references SappOrchestra | ✅ verified by grep |
| Core API framework-independent | ✅ no JUCE in public headers |
| Realtime safety preserved | ✅ allocation-guard tests in both repos' scope |
| Presets compatible | ✅ v1 schema defined in SappOrchestra only |
| Documentation explains the relationship | ✅ this file + both architecture.md |
