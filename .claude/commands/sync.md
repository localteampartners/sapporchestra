---
description: Adaptive drift check — sample git first, re-read only docs likely affected.
---

Find drift between the project docs and reality using as few tokens as possible. Don't open files for the sake of it.

## Delegate to sapptest if available

If `_test/prompts/docs-drift.md` exists and is not the scaffold
placeholder, read and follow that prompt instead — it's the upstream
sapptest version of this check, with richer schema awareness. Otherwise
use the steps below (sappcode's built-in fallback).

## Steps

1. **Sample reality first (cheap).** Run in parallel:
   - `git log --oneline -30`
   - `git status --short`
   - `ls` at project root

   Look at the output. **Do not open any docs yet.**

2. **Classify what changed.** For each recent commit, ask: which doc(s) *might* have drifted?

   | Change signal | Doc(s) to flag |
   |---|---|
   | Feature shipped (`feat:`, `ship:`) | `CURRENT_STATE`, `CHANGELOG`, `TODO` |
   | Bug fix (`fix:`) | `CURRENT_STATE` (known issues section) |
   | Env var touched (`.env.example` or `.env.template` in diff) | `ENVIRONMENT`, `.env.example`, `.env.template` |
   | New external service / SDK (package.json / pyproject.toml changes) | `DEPENDENCIES`, `ARCHITECTURE` |
   | Build / deploy config touched | `RUNBOOK` |
   | Deploy script / infra file touched | `INFRASTRUCTURE`, `RUNBOOK` |
   | Cluster of commits in one theme over weeks | `ROADMAP` (theme may be ready to move Now → Next, or "Now" item finished) |
   | Nothing interesting | skip entirely |

   Build a **minimal** set of files to open. If git is quiet, the set may be empty.

3. **Always check CURRENT_STATE staleness.** Open `_project/CURRENT_STATE.md` and look at the "Last verified" date. If > 14 days old in an active project, flag it.

4. **Open only the flagged docs.** Compare each against the git evidence. Look for:
   - Features listed as "in progress" that actually shipped
   - Commands no longer valid
   - Env vars in code but not in ENVIRONMENT.md
   - `<!-- FILL IN: -->` placeholders still present in an active project

5. **Report drift.** Group by file. For each, show before/after edits. **Do not apply yet.**

6. **Ask which to apply.** Per file or all. Apply only what's approved.

## Output format

```
## Drift report

Git: 12 commits since last sync. Flagged docs: CURRENT_STATE, CHANGELOG, ENVIRONMENT.
(Others look fine based on git evidence — not opened.)

### _project/CURRENT_STATE.md
- "Last verified" is 2026-01-15 (98 days ago).
- Lists auth as "in progress" but commit abc123 shipped it.
  Proposed: move to "built and working".

### _project/ENVIRONMENT.md
- REDIS_URL read in src/cache.ts but not documented.
  Proposed: add row to Required table.

### _project/CHANGELOG.md
- No entry for auth ship.
  Proposed: append "YYYY-MM-DD — Auth flow shipped."

Apply all? Or per-file?
```

## Rules

- **Don't open docs you don't need.** Git evidence drives what you open.
- **Don't apply edits without approval.**
- **If nothing changed, say "no drift — git is quiet" and stop.** Don't read docs for the sake of it.
