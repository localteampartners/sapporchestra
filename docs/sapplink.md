# SappLink CC-in (sapporchestra)

SappOrchestra implements SappLink v1 MIDI CC-in so sapptune-generated clips
can drive its parameters. Protocol: `~/apps/sapptune/sapplink/PROTOCOL.md`.
**Source-of-truth manifest:** `~/apps/sapptune/sapplink/manifests/sapporchestra.json`
(authored 2026-08-06 directly from the plugin's real APVTS ranges — no
corrections pending on either side). A vendored copy lives at
[tests/data/sapplink-manifest.json](../tests/data/sapplink-manifest.json);
`tests/unit/test_sapplink.cpp` fails if the in-code table and the vendored
manifest ever drift. If sapptune's manifest changes, update the vendored copy
and `src/core/SappLinkCCMap.cpp` together.

## The mapping

| CC | Parameter ID | Range (engineering) | Curve |
|---|---|---|---|
| 7 | `masterGain` | −24 … 12 dB | linear |
| 14 | `earlyLevel` | 0 … 1 | linear |
| 15 | `hallDecay` | 0.3 … 12 s | log |
| 16 | `stageX` | −1 … 1 | linear |
| 17 | `stageDepth` | 0 … 1 | linear |
| 18 | `width` | 0 … 2 | linear |
| 19 | `hallDamping` | 0 … 1 | linear |
| 26 | `dnaAmount` | 0 … 1 | linear |
| 68 | `legato` | 0 … 1 (≥0.5 on) | linear |
| 91 | `tailLevel` | 0 … 1 | linear |
| 92 | `hallSize` | 0.2 … 1.5 | linear |

CC 0→127 maps onto the range through the curve (log = exponential
interpolation between endpoints; linear = lerp). CCs are accepted on any
MIDI channel.

## Deliberately NOT in the mapping (existing behavior preserved)

- **CC 1 → dynamics** and **CC 11 → expression** — engine-native performance
  controls (OrchestraEngine live-follows them); clips should keep using them
  per the SappLink standard-CC conventions.
- **CC 64** — real sustain-pedal semantics (deferred releases) in SappSounds.
  (CC 68 legato-footswitch IS mapped — it toggles the `legato` parameter,
  matching its MMA meaning.)
- **Pitch bend** — voice pitch, per `midi.pitchBendRangeSemitones`.
- **Keyswitch notes** — articulation switching stays note-based; the per-
  instrument keyswitch table is discoverable via `sapporchestra inspect`.
- Discrete/config parameters (`dnaMode`, `quality`, `limiter`,
  `articulation`) are host-automation/CLI-only.

## How it's routed

- **Plugin path** (`src/plugin/PluginProcessor.cpp`): mapped CCs become slew
  targets; each block the APVTS parameter moves ~15 ms toward the target via
  `setValueNotifyingHost` — the same normalized path host automation uses,
  never straight into the DSP — so 7-bit steps don't zipper and the UI/host
  see the motion. The CC event is still forwarded to the engine (SFZ
  `locc/hicc` conditions keep working).
- **CLI / offline path** (`src/core/OrchestraRender.cpp`): the same table
  updates `OrchestraParams` during `renderOrchestra`, so a sapptune clip
  renders identically through `sapporchestra render`. The engine's built-in
  parameter smoothing de-zippers this path.
- One table drives both: `src/core/SappLinkCCMap.{h,cpp}` (framework-free).

## Where the mapping lives in the Sapp stack

SappOrchestra owns it (parameters are product policy). **SappSounds has no
SappLink code by design** — the engine stays product-neutral and already
forwards raw CC state to SFZ region conditions. sappsynth implements its own
manifest in its own repo.

## Verification

- `SappOrchestraTests "[sapplink]"` — table↔manifest drift guard, curve
  endpoints/monotonicity, reserved CCs, offline proofs (CC 16 flips the
  stereo stage; CC 7 scales output level).
- `SappOrchestraUiShot --cctest` — plugin-path proof through
  `processBlock`: prints `PASS` when CC 16 pans the rendered image.
