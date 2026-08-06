# CHANGELOG — sapporchestra

<!-- UPDATE WHEN: a feature ships or a meaningful fix lands -->

## 2026-08-06 — v0.3.0
- Legato level 2 shipped: legato parameter (default on), CC 68
  (MMA legato footswitch) in the SappLink manifest, UI toggle; engine
  mechanism in SappSounds v0.3.0 (attack suppression + transition fades,
  chord-guarded)
- CC1 dynamics now truly morph dynamic layers on crossfade-capable
  instruments (SappSounds xfin/xfout); Diagnostic Orchestra demonstrates it

## 2026-08-06 — v0.2.0
- Real-library milestone: Sonatina Symphonic Orchestra loads and plays
  (FLAC via SappSounds v0.2.0; 8-articulation keyswitch patches verified)
- CLI: scan (library discovery JSON), seats (orchestral seating templates)
  + render --seat; params in stems demo
- scripts/make_sonatina_demo.py: 7 real sections seated in one hall

## 2026-08-06 — v0.1.1
- SappLink v1 MIDI CC-in: table-driven map (src/core/SappLinkCCMap) of 10
  CCs to stable parameter IDs per sapptune's sapporchestra.json manifest
  (authored this session from the plugin's real ranges). Plugin routes CCs
  through APVTS with ~15 ms slew; CLI/offline renders honor the same CCs.
  Drift-guard test vs vendored manifest; render + plugin-path proofs.

## 2026-08-06 — v0.1.0
- First working build: OrchestraEngine (CC1 dynamics / CC11 expression /
  stage / ER + FDN hall / Analog DNA / limiter), JUCE Standalone + VST3 + AU
  with concert-hall UI (articulation chips, stage pad, keyswitch keyboard,
  meters), async SFZ loading with Diagnostic Orchestra fallback, host state
  v1, agent CLI (inspect/validate/params/render), demo stem pipeline,
  12-case test suite.
