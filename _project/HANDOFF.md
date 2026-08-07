# HANDOFF — sapporchestra

<!-- UPDATE WHEN: pausing work, finishing a session, or another agent/session takes over -->

**Last session:** 2026-08-06 — v0.5.0 released.

## Where things stand

Multitimbral orchestra is complete: 16 MIDI-channel slots, per-slot mixer
(vol/mute/solo), per-slot stage seats, shared hall, Instruments browser with
configurable shared samples folder, GET SOUNDS downloads, and the one-click
FULL ORCHESTRA Sonatina preset (all 16 channels seated + balanced). 22 tests
green; releases carry Windows-x64 + macOS-universal zips automatically via
the release-builds workflow (fires on every tag).

## Watch out for

- Stage APVTS params are selected-slot-scoped; the processor writes through
  on change only (so per-channel CC16/17/18 aren't clobbered). selectSlot
  reflects the slot's seat back into APVTS — mind the lastStage caches.
- SappLink table/manifest must stay in sync (drift-guard test; sapptune repo
  holds the source of truth; CC16/17/18 are engine-side per-channel).
- Other sessions own ~/apps/sappsynth and sapptune's engine/ — don't touch.

## Next obvious work

_project/TODO.md "Next" list: DAW validation pass (needs a human at Logic/
Reaper), missing-library relocate dialog, X-Ray panel, more presets,
per-slot audio outputs.
