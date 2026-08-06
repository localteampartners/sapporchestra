# SPEC — sapporchestra

<!-- UPDATE WHEN: goals change, scope changes, non-goals change, or the target user changes -->

## What this is

A professional orchestral sample instrument (JUCE Standalone/VST3/AU) built
on the SappSounds engine. Loads user-selected local SFZ libraries (VPO, VSCO
CE, Sonatina, custom) and turns them into a coherent performance instrument:
first-class articulations, CC1 dynamics distinct from CC11 expression, stage
placement, early reflections + shared hall, subtle Analog DNA, and a JSON
agent CLI so MIDI-generation software can inspect instruments and render
deterministic audio.

## Why it exists

Free orchestral libraries sound like WAV players without an engine around
them. The differentiator is not gigabytes: correct sample behavior,
expressive control, clear articulations, coherent space, inspectable
internals. Also: Michael's MIDI-generation software needs a programmable
orchestral renderer — the agent CLI is a core deliverable, not an add-on.

## Goals

- Musical out of the box (built-in Diagnostic Orchestra, strong defaults)
- Reliable articulation switching (UI, parameter, keyswitch — one state)
- Deterministic renders for agent workflows (seeded)
- Stable parameter/state contracts (IDs versioned from day 1)

## Non-goals (v1)

- DAW/notation/score features; Kontakt compatibility; sample store
- Full multitimbral rack & section buses (after single-instrument is solid)
- Automatic AI orchestration inside the plugin (external agents do this via
  the CLI)
