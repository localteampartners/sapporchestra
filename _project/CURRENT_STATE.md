# CURRENT STATE — sapporchestra

<!-- UPDATE WHEN: a feature ships, something breaks, or a known issue is found/fixed -->

**As of 2026-08-06 — v0.5.0: full multitimbral orchestra.**

## Working

- FULL ORCHESTRA one-click presets: Sonatina AND Virtual Playing Orchestra
  (VPO brass = DXF dynamic-crossfade patches), 16 sections seated + balanced
  across 16 MIDI channels (Instruments panel buttons; sequential loading)
- Multitimbral rack: 16 channel slots (per-slot instrument/stage/CCs +
  volume/mute/solo mixer, shared hall, omni fallback, channel strip UI,
  state v2)

- OrchestraEngine: CC1 dynamics (level+timbre), CC11 expression, stage
  placement (pan/width/depth), early reflections + 8-line FDN hall, Analog
  DNA (detune/drift/vintage noise), quality modes, soft limiter, articulation
  switching by keyswitch injection
- JUCE plugin builds: Standalone, VST3, AU (auto-copied to ~/Library/Audio/Plug-Ins)
- UI: articulation chips w/ keyswitch names + live highlight, stage pad,
  dynamics/expression/hall knobs, keyswitch-colored keyboard, voice count +
  peak meter, SFZ file loading, built-in Diagnostic Orchestra
- Host state save/restore (APVTS v1 + sfzPath, diagnostic fallback)
- Agent CLI (`sapporchestra`): inspect / validate / params / render (JSON,
  deterministic seeds)
- Tests: 12 cases green (engine policy, room, deterministic renders)
- Demo pipeline: scripts/make_demo.py + scripts/make_sonatina_demo.py
  (real 7-section Sonatina piece: seated stems, shared hall)
- CLI scan (library discovery) + seats (orchestral seating templates,
  render --seat)
- SappLink CC-in: 10 CCs → parameters (manifest-driven, drift-guarded),
  plugin slew path + CLI/offline path; CC1/CC11/CC64 stay engine-native

## Known issues / limits

- Not yet validated in a DAW session (Reaper/Logic/Live) or with pluginval/auval
- VPO download link not yet sourced (Sonatina 747-file library fully working)
- Library browser shipped (Instruments panel: shared samples folder,
  categories, filter, prev/next stepping, one-click downloads)
- Missing-library relink UI minimal (falls back to diagnostic + status text)
- Sample streaming pending (inherits SappSounds full-RAM preload)
- CLI is single-instrument per render (stems for arrangements)
