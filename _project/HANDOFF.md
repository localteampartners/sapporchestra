# HANDOFF — sapporchestra

<!-- UPDATE WHEN: pausing work, finishing a session, or another agent/session takes over -->

**Work in flight:** none. v0.9.0 committed and pushed; NO TAG (the self-hosted
Windows build host is being re-set-up — do not tag until it is back).

## Where things stand

Issues #1 and #2 are fixed and closed, and the suite-wide `clean` control
(sapptune #30) is in.

- **#2 root cause:** every instrument install was delivered on the JUCE
  message thread (`juce::Timer` + `MessageManager::callAsync`). A headless
  non-JUCE host has a MessageManager nobody pumps, so the parameter write was
  stored and never applied. Secondary: `finishLoad`'s generation guard never
  returned, so the construction diagnostic could overwrite a real load.
  Loading now runs on the processor's own loader thread.
- **#1:** `SAPP_SFZ_RESCAN=1`, plus `sapporchestra-headless index --rescan`
  and `sapporchestra sfz-index --rescan`; both binaries ship in the release
  zip and packaging fails if either is missing.
- New: `libraryReady` host parameter, `SappOrchestra-audio-source:` logging,
  `sapporchestra-headless` station harness (CTest + verify.sh).

37 unit cases + the 16-check headless selftest, all green; `verify.sh` now
builds the plugin target.

## Watch out for

- Never route a load through `juce::Timer` / `MessageManager::callAsync`
  again — see _project/DECISIONS.md, 2026-08-09.
- The supersede guard is PER SLOT (`slotGeneration_`); a global one silently
  drops 15 of a 16-slot state restore. The headless selftest catches that.
- SappLink table / vendored manifest / docs/sapplink.md move together.
  sapptune owns the upstream manifest — CC 3 = `clean` needs to land there.

## Next obvious work

_project/TODO.md: a note-on gate (sappkeys' StartupGate) so a render started
before the selection lands cannot sound the built-in default; a `clean`
control in the editor; DAW validation.
