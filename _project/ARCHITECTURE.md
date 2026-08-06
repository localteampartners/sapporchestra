# ARCHITECTURE — sapporchestra

<!-- UPDATE WHEN: stack changes, components change, data flow changes -->

Authoritative: [../architecture.md](../architecture.md). Quick facts:

- Product layers: `src/core` (framework-free orchestra policy: dynamics,
  expression, stage, ER + FDN hall, DNA, offline render) → `src/plugin`
  (JUCE APVTS/processor/editor) and `src/cli` (JSON agent CLI)
- Engine dependency: sibling repo `../sappsounds` via
  `add_subdirectory` (`Sapp::Sounds`); FetchContent fallback
- JUCE 8.0.15 pinned (same as sappsynth); Standalone + VST3 + AU
- Parameter IDs and state schema v1 are compatibility contracts
- UI: custom LookAndFeel ("concert hall at night"), stage XY pad,
  articulation chips, keyswitch-aware keyboard, offscreen UiShot tool
