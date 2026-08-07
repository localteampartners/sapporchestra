# TODO — sapporchestra

<!-- UPDATE WHEN: tasks are added, completed, or reprioritized -->

## The "top of the line" master list

### Done (2026-08-06)
- [x] Real library end-to-end (Sonatina, 745/747 instruments; FLAC decode)
- [x] CC1 dynamic-layer crossfade (live xfin/xfout morphing in SappSounds)
- [x] Legato level 2 (attack suppression + transition fades, CC 68, UI toggle)
- [x] SappLink CC-in (11 CCs, manifest-driven, drift-guarded)
- [x] Agent CLI: inspect/validate/params/seats/scan/render --seat
- [x] One-command sample downloads (sappsounds scripts/fetch-library.sh)
- [x] VPO validated: 454/454 playable incl. 247 DXF crossfade patches

## Next (make it top of the line)

- [ ] DAW validation: Logic auval, Reaper, Live; pluginval in CI
- [ ] In-plugin library browser fed by the scanner (search, categories,
      favorites, recent, RAM estimate before load)
- [ ] Missing-library relocate dialog (Locate / Search common paths / Disable)
- [ ] X-Ray panel in the editor (selection inspector + mapping view — the
      DiagnosticSnapshot feed already carries the data)
- [x] FULL ORCHESTRA factory preset (16 Sonatina sections, seated+balanced)
- [x] VPO Full Orchestra preset (DXF brass)
- [ ] More factory presets: Film/Chamber seatings, string-only
- [ ] True round-robin reset + repeated-note policy controls in UI
- [ ] Per-slot audio outputs (multi-bus) for DAW-side mixing

## Later (state of the art)

- [ ] Disk streaming (SappSounds docs/streaming.md) for giant libraries
- [ ] Convolution hall option (IR loading off-thread, partitioned)
- [ ] Legato level 3: recorded transition samples when libraries provide them
- [ ] MPE per-note expression (pressure→dynamics, slide→brightness)
- [ ] Section buses + multi-instrument rack (full orchestra in one instance)
- [ ] Windowed-sinc resampling quality tier + offline Ultra mode
- [ ] Guided experiments (A/B teaching flows from the architecture doc)
- [ ] Installer/notarization + GitHub Actions CI (build matrix, tests,
      pluginval, release artifacts)
- [ ] sapptune deep integration: articulation planning from inspect data,
      auto-seating from arrangement roles, per-phrase seed retakes
