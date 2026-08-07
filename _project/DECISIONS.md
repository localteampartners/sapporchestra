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

## 2026-08-06 — SappLink CC map lives here, not in SappSounds
The CC→parameter contract is product policy (parameters are the product's),
so src/core/SappLinkCCMap owns it and both the plugin and the offline render
consume the same table. SappSounds stays SappLink-free: the engine only
forwards raw CC state to SFZ region conditions. CC 1/11/64 are engine-native
performance controls and are excluded from the mapping. The sapporchestra
manifest was authored directly from the real APVTS ranges, so plugin and
manifest agree with zero corrections.

## 2026-08-06 — Multitimbral as 16 fixed slots keyed by MIDI channel
No dynamic slot management: slot N ≡ channel N+1, always allocated (voices
are cheap; instruments load on demand). Omni fallback while ≤1 slot is
occupied keeps single-instrument workflows and ch-1 keyboards working.
Stage/dynamics/expression are per-slot (driven per channel); the hall,
master, DNA, legato, quality stay global — one room, one conductor.

## 2026-08-06 — Factory preset = search-by-filename over the library
The FULL ORCHESTRA preset finds Sonatina's section patches by filename
anywhere under the samples root instead of hardcoding paths — resilient to
how the library was unpacked. Missing files skip their slot gracefully.
