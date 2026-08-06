---
description: Show which registered repos have uncommitted changes — i.e. the ones the nightly --skip-dirty run would skip.
---

Run `sapp-snapshot dirty $ARGUMENTS` and show the output.

This answers "which projects have work I need to commit/stash so the next nightly snapshot covers them?" The nightly launchd job at 3:30 AM uses `--skip-dirty`, so any repo listed here gets skipped that night.

Flags the user may pass:
- `-v` / `--verbose` — show the first few changed files in each dirty repo (helps decide what to do).
- `--json` — machine-readable output.

If the user asks "what do I need to fix before the next snapshot?", run this and walk them through the dirty repos one at a time, asking whether each should be committed, stashed, or discarded.
