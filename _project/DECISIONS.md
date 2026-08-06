# DECISIONS — sapporchestra

<!-- UPDATE WHEN: a non-obvious technical choice is made -->

## 2026-08-06 — Product/engine split before code
Generic sampler code lives in the sibling SappSounds repo (`Sapp::Sounds`);
this repo only holds orchestra policy, JUCE wrappers, UI, CLI. No SappAudio
umbrella. See docs/boundary_report.md.

## 2026-08-06 — Knob→CC bridging for dynamics/expression
CC1/CC11 are persistent live overrides in the engine; moving the UI knobs
injects the matching CC instead of fighting it. One state, three inputs
(host CC, host automation, UI).

## 2026-08-06 — Articulation switching = keyswitch injection
UI/parameter articulation changes inject the articulation's keyswitch note
into the event stream. The engine keyswitch state stays the single source of
truth; MIDI-file keyswitches, automation, and clicks can't diverge.

## 2026-08-06 — Agent API as a CLI, not a socket
`sapporchestra` subcommands with JSON stdout: deterministic, testable,
CI-friendly, trivially callable from any language. A live OSC/socket API can
come later without breaking this contract.

## 2026-08-06 — JUCE 8.0.15 pinned
Matches sappsynth (stay on last stable 8.x until 9.x settles).

## 2026-08-06 — Algorithmic hall first (8-line Householder FDN)
Convolution is optional/later; the FDN gives a controllable, coherent shared
tail with predictable CPU.
