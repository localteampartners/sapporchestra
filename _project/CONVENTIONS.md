# CONVENTIONS — sapporchestra

<!-- UPDATE WHEN: you learn (or are corrected on) a non-obvious workflow fact — a deploy quirk, version pin, build gotcha, naming rule, or "never do X here." If a session had to rediscover it, it belongs in this file. -->

How we work on this project — facts that aren't derivable from the code and
that every new session would otherwise relearn the hard way. One or two lines
per entry: the rule, then the why (when the why isn't obvious).

## Deploy & operations

- No server. "Deploy" = a GitHub release: bump `project(... VERSION ...)` in
  CMakeLists.txt, commit, push, then tag. The tag triggers
  `.github/workflows/release-builds.yml`, whose version guard fails the run if
  CMakeLists lags the tag (the in-plugin updater compares the two, so a lag
  re-offers the same update forever).
- The build host is self-hosted (`vars.WINDOWS_RUNNER`). When it is down, ship
  nothing: commit and push, but do NOT create the tag.

## Toolchain & versions

- JUCE 8.0.15, pinned (matches sappsynth). Catch2 v3.7.1.
- Needs the sibling `../sappsounds` checkout (or `-DSAPPSOUNDS_DIR=…`).
- CMake ≥ 3.24, C++20.

## Build / test gotchas

- `verify.sh` builds the PLUGIN target deliberately. Never verify with
  `-DSAPPORCHESTRA_BUILD_PLUGIN=OFF`: sappkeys#1's postmortem was tests green
  while the installed binary stayed stale, and the headless regression needs
  the real processor.
- The headless regression (`sapporchestra-headless selftest`) is the only test
  that catches message-loop-dependent bugs. Catch2 cases cannot: they never
  construct the JUCE processor.
- macOS builds are LOCAL VERIFICATION ONLY. Releases ship Windows artifacts
  (suite rule) — never attach a macOS zip.

## Code & naming rules

- New host parameters are APPENDED, never inserted: every existing automation
  index is a compatibility contract. Order today: …, `instrument`, `clean`,
  `libraryReady` (the last one lives outside the APVTS).
- When the SappLink table changes, update `src/core/SappLinkCCMap.cpp`, the
  vendored `tests/data/sapplink-manifest.json` and `docs/sapplink.md` in the
  same edit — the drift-guard test fails otherwise. sapptune owns the
  upstream manifest; tell that repo separately.

## Never do

- Never make an instrument load depend on the host pumping a JUCE message
  loop (`juce::Timer`, `MessageManager::callAsync`). A headless VST3 host has
  a MessageManager that nobody pumps: the work is accepted and silently
  dropped, and the plugin plays its default sound with no error anywhere.
  That was issue #2. Loads go through the processor's loader thread.
- Never capture `this` in a `callAsync` closure from an async load — a
  destroyed instance's queued closures crash on the next pump.
