# DECISIONS — sapporchestra

<!-- UPDATE WHEN: a non-obvious technical choice is made -->

## 2026-08-09 — Instrument loading never touches the JUCE message thread
A VST3 plugin inside a non-JUCE headless host has a MessageManager that
nothing pumps, so `juce::Timer` callbacks and `MessageManager::callAsync`
never run there. Anything routed through them is accepted and silently
dropped (issue #2). The processor therefore owns a loader thread: parameter
selections, program changes, state restores and factory presets all become
queued `LoadJob`s, and the install happens on that thread. The 30 Hz timer is
now an editor convenience — if it never fires, nothing about the sound
changes. Rule for this repo: **a load path that only works when the host
pumps a message loop is a bug, not a design.**

## 2026-08-09 — `libraryReady` lives outside the APVTS
It is a status readout, not part of the sound. Inside the APVTS,
`copyState`/`replaceState` would save and restore it, and a session restored
with a stale "ready" would lie to a headless host at the worst moment. Same
call sappkeys made in its v0.8.0.

## 2026-08-09 — `clean` scales `dnaAmount`, and nothing else
Audited every source of modeled imperfection in the engine. Analog DNA is the
only one, and its three expressions (per-note random detune, slow gain drift,
vintage hiss) all read `dnaAmount`, so the contract is one helper,
`effectiveDnaAmount()`, applied at each use site. Hall modulation is NOT
scaled: it is an FDN device that stops the tail ringing metallically — room
design, not wear. Round-robin and velocity variation are NOT scaled: they
come from the sample library, not from us. Default 0 keeps renders
byte-identical to pre-`clean` builds, which the test suite asserts.

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
