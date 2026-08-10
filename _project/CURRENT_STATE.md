# CURRENT STATE — sapporchestra

<!-- UPDATE WHEN: a feature ships, something breaks, or a known issue is found/fixed -->

**As of 2026-08-09 — v0.9.0: headless selection actually loads (#2), headless SFZ index (#1), suite-wide `clean`.**

## Working

- Six one-click presets: three orchestras (Sonatina / VPO / VSCO2 CE)
  plus DRUMS + PERC, PIANOS + KEYS, CHOIR + VOICES (multi-library slots)
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
- Tests: 38 cases green (engine policy, room, deterministic renders,
  SappLink drift guard, SFZ library index/order, `clean` contract) plus the
  headless station selftest (16 checks) under CTest
- Demo pipeline: scripts/make_demo.py + scripts/make_sonatina_demo.py
  (real 7-section Sonatina piece: seated stems, shared hall)
- CLI scan (library discovery) + seats (orchestral seating templates,
  render --seat)
- SappLink CC-in: 12 CCs → parameters (manifest-driven, drift-guarded),
  plugin slew path + CLI/offline path; CC1/CC11/CC64 stay engine-native
- Headless-safe instrument loading (issue #2): every install runs on the
  processor's own loader thread, so selection works with no GUI, no user and
  no host message loop. `libraryReady` host parameter (read-only) says when a
  selection has landed; `SappOrchestra-build/-instrument/-audio-source` log
  lines name what actually sounded (`SAPP_ORCHESTRA_LOG=<file>` to capture)
- Headless SFZ index (issue #1): `SAPP_SFZ_RESCAN=1` rebuilds the index at
  construction; `sapporchestra-headless index --rescan` and
  `sapporchestra sfz-index --rescan` do it ahead of time. Both binaries ship
  in the release zip (packaging fails if either is missing)
- `sapporchestra-headless` station harness (console app, all platforms):
  `selftest` (the #1/#2 regression, run by CTest and verify.sh), `render`,
  `index`
- SappLink `clean` (CC 3, sapptune #30): scales every modeled imperfection by
  (1 - clean); default 0 = exactly the previous behavior
- Host-automatable SFZ selection (sapptune #20): `instrument` choice param
  (appended last, automation indices hold) enumerates the library via
  `<samplesRoot>/.sapp-sfz-index.json` (ordering contract in
  src/core/SfzLibrary, case-insensitive by label); MIDI bank-select +
  program change loads by entry index into the channel's slot; state stays
  path-based; CLI `sfz-index` prints name→choice→normalized; rescans take
  effect next instantiation

## Known issues / limits

- No note-on gate: a host that renders BEFORE the selection has landed hears
  the built-in default for that render. Poll `libraryReady` (or use a settle
  window) — sappkeys' StartupGate is the fuller answer if this ever bites.
- `clean` has no editor control yet (host parameter + CC 3 only)
- Not yet validated in a DAW session (Reaper/Logic/Live) or with pluginval/auval
- VPO download link not yet sourced (Sonatina 747-file library fully working)
- Library browser shipped (Instruments panel: shared samples folder,
  categories, filter, prev/next stepping, one-click downloads)
- Missing-library relink UI minimal (falls back to diagnostic + status text)
- Sample streaming pending (inherits SappSounds full-RAM preload)
- CLI is single-instrument per render (stems for arrangements)
