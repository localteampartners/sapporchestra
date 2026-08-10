# CHANGELOG — sapporchestra

<!-- UPDATE WHEN: a feature ships or a meaningful fix lands -->

## 2026-08-09 — v0.9.0: the `instrument` parameter actually loads (#2), headless index (#1), `clean`

**Fixed (#2) — the `instrument` parameter was accepted and then did nothing
in a headless host.** Two independent faults, either one enough on its own:

1. *Every* instrument install was delivered on the JUCE message thread — the
   selection was applied from a `juce::Timer` callback and the loaded
   instrument was installed from `MessageManager::callAsync`. A VST3 plugin
   inside a non-JUCE headless host has a MessageManager that nobody pumps, so
   neither ever ran: the parameter write landed in `pendingInstrumentChoice_`
   and stayed there. No error, no log, default sound. Instrument loading now
   runs on a loader thread the processor owns, so it works with no GUI, no
   user and no message loop. The 30 Hz timer survives only as an editor hook.
2. `finishLoad` computed its generation guard and never returned on a miss,
   so the slow construction-time diagnostic could land *after* a real SFZ and
   overwrite it — and then reset `instrument` to "(keep current)". It returns
   now, and the construction diagnostic never writes the parameter at all.

Also fixed by the same rework: destroying an instance with a load in flight
left `callAsync` closures capturing `this`, which crashed when a later pump
ran them. The loader thread is joined in the destructor.

- **`libraryReady` host parameter** (read-only, non-automatable, appended
  last, outside the APVTS so host state never saves a stale "ready"): 0 the
  instant a selection is written, 1 when that instrument is installed. Lets a
  headless host poll instead of using a blind settle window. Mirrors sappkeys
  v0.8.0.
- **`SappOrchestra-audio-source:` log line** names the instrument that
  produced a voice batch started from silence (plus `SappOrchestra-build:` at
  construction and `SappOrchestra-instrument:` per install). This is how the
  fault class becomes visible in the wild instead of only audible. Set
  `SAPP_ORCHESTRA_LOG=<file>` to capture them; on Windows they also go to
  stderr, where the JUCE logger alone would not be greppable.

**Fixed (#1) — headless SFZ index rebuild.** `SAPP_SFZ_RESCAN=1` rebuilds
`<root>/.sapp-sfz-index.json` at plugin construction, before the choice list
is built. The release zip now also ships `sapporchestra.exe` (`sfz-index
--rescan`) and the new `sapporchestra-headless.exe` (`index --rescan`), and
the packaging step FAILS if either binary is missing. A missing index is
still scanned and written automatically at first instantiation. Exact
invocations in _project/RUNBOOK.md.

- **New `sapporchestra-headless` target** (cross-platform console app): the
  station harness. `selftest` is the #1/#2 regression — it renders the same
  MIDI with and without the `instrument` parameter set, with no dispatch
  loop, and asserts the audio DIFFERS and that the named instrument is what
  sounded. Registered with CTest; `verify.sh` now builds the plugin target
  and runs it (sappkeys#1: tests green while the installed binary was stale).

**Added — `clean` (sapptune #30, suite-wide).** New host parameter `clean`
("Clean", 0..1, default 0, CC 3 reserved), appended after `instrument`.
Scales every modeled-imperfection source by (1 − clean). Audited: Analog DNA
is the only such source here, and all three of its expressions (per-note
random detune, slow gain drift, vintage hiss) are driven by `dnaAmount`, so
`clean` scales that. Not scaled: hall modulation (FDN anti-metallic device)
and round-robin variation (library-supplied). Default 0 is byte-identical to
the previous behavior. CLI: `--param clean=…`.

- Tests: 38 unit cases (was 30) + the headless selftest (16 checks),
  including the station's hard-won indexing rules: a file whose regions
  arrive only through `#include` (sneakybass, "All Brass KS") is playable,
  and the index is UTF-8 with no BOM.

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
