# CHANGELOG — sapporchestra

<!-- UPDATE WHEN: a feature ships or a meaningful fix lands -->

## 2026-08-09 — host-automatable `instrument` parameter (sapptune #20)
- New `instrument` AudioParameterChoice (appended LAST — all existing
  automation indices hold): enumerates every installed SFZ instrument from
  a cached index at `<samplesRoot>/.sapp-sfz-index.json`; selecting choice
  k loads library entry k-1 into the selected slot (message thread).
- New core module `SfzLibrary` (no JUCE): scan / index / ordering contract
  — entries sorted case-insensitively by label; the plugin, CLI and index
  file always agree on the order. `SAPP_SFZ_ROOT` env overrides the root.
- MIDI bank-select (CC0/CC32) + program change selects by entry index into
  the receiving channel's slot: entry = (bank * 128) + program.
- Chosen SFZ still persists BY PATH in host state (graceful fallback when
  the file is gone); the parameter re-syncs to the loaded path.
- CLI: `sapporchestra sfz-index [--root DIR] [--rescan]` prints the
  entry/choice/normalized table a driving session needs; rescans take
  effect on the next plugin instantiation (choice lists are fixed live).
- Manifest: `hostParameters` + `instrumentSelect` contract in
  sapptune/sapplink/manifests/sapporchestra.json (mirrored in tests/data).
- Verified: 30 unit tests green (new [sfzlib] cases incl. sort-order and
  index round-trip), headless `--sfztest` (11 checks), auval PASS.

## 2026-08-07 — v0.7.0
- In-plugin UPDATE button: the editor checks the GitHub latest release
  (throttled to once a day; click the version label to check on demand).
  When a newer version exists an UPDATE button appears — one click
  downloads the right platform build, installs it (macOS: into
  ~/Library/Audio/Plug-Ins with quarantine cleared; Windows: replaces the
  loaded .vst3 via the rename trick), and the standalone app relaunches
  itself updated. Inside a DAW the update lands on disk and the plugin
  says "INSTALLED - REOPEN" (hosts own the loaded binary).
- Plugin version now tracks release tags (0.7.0), enabling the comparison.

## 2026-08-07 — v0.6.0
- Three FAMILY presets join the three orchestras: DRUMS + PERC (AVL kits,
  VSCO mallets/timpani, Sonatina percussion), PIANOS + KEYS (Salamander,
  FreePats uprights/FM, Sonatina grand/harpsichord/organ, VSCO keys,
  harps), CHOIR + VOICES (Sonatina choruses, VPO choirs, synth pad choir,
  legato solo voice). Preset slots now target libraries individually
  (multi-library presets; missing libraries skip their slots and fill in
  once downloaded). Library lookups cached.

## 2026-08-07 — v0.5.2
- FULL ORCHESTRA - VSCO2 CE: third factory preset — the chamber-scale
  Versilian orchestra on 16 channels (violin/viola/cello ensembles + solo
  violin desk, solo winds, brass, timpani, harp, glockenspiel). Enabled by
  a SappSounds parser fix (positional default_path) that makes VSCO's KS
  combo patches fully playable, plus GET SOUNDS now downloading VSCO's
  samples AND its SFZ mappings (separate branches) and merging them.

## 2026-08-07 — v0.5.1
- FULL ORCHESTRA - VPO: second factory preset loads the complete Virtual
  Playing Orchestra across all 16 channels (same classic seating); brass
  slots use the DXF patches, so CC1 morphs real recorded dynamic layers.
  The Instruments panel now shows one FULL ORCHESTRA button per library.

## 2026-08-06 — v0.5.0
- FULL ORCHESTRA preset: one click in the Instruments panel loads the
  complete Sonatina orchestra across all 16 MIDI channels — strings I/II/
  violas/celli/basses, flutes/oboes/clarinets/bassoons, horns/trumpets/
  trombones/tuba, timpani, harp, chorus — each seated classically and
  balance-trimmed. Instruments stream in sequentially with live status.
- Instruments browser: header shows which channel a double-click loads into.
- Shared settings helper (SappSettings.h) for the family-wide samples folder.

## 2026-08-06 — v0.4.0
- Per-slot mixer: volume (−60…+12 dB), mute, and solo per MIDI-channel slot
  (MIX strip under the channel grid; solo silences all non-soloed slots;
  states persist in the session and color the channel numbers)
- MULTITIMBRAL: 16 MIDI-channel slots in one instance — track 1/ch 1 →
  violin, track 2/ch 2 → cello, etc. Each slot: own instrument, stage seat,
  early reflections, per-channel CC1/CC11 phrasing and CC16/17/18 stage;
  all slots share one hall + master. Omni fallback while only one slot is
  loaded. Channel strip UI; loads/articulations/stage pad target the
  selected slot; state schema v2 (per-slot paths + stages, v1 migrates).
- Instruments browser: the GET SOUNDS panel is now the full instrument
  chooser — configurable samples folder (persisted in the shared
  Sapp/SampleLibraries settings so every Sapp instrument uses the same
  root), category dropdown, filter, double-click to load; header
  instrument name is clickable and ◀ ▶ arrows step through everything
  installed. One-click library downloads (VPO/Sonatina/VSCO2) built in.

## 2026-08-06 — v0.3.1
- Virtual Playing Orchestra validated end-to-end (454/454; DXF dynamic
  crossfade patches morph real recorded pp/ff layers on CC1)
- scripts/make_vpo_demo.py: 7-section VPO piece (DXF horns swelling)
- TODO: VPO validation checked off the master list

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
