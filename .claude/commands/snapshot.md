---
description: Take a snapshot of the current project (default), every project (--all), or a subset (--only). Optionally include a friendly name.
---

Run `sapp-snapshot create $ARGUMENTS` and report the snapshot id and any push failures back to the user.

**Default behavior:** snapshots **just the current project** — the registered repo the user's cwd is inside. This is the 95% case.

**Flags the user may pass via `$ARGUMENTS`:**

- `--all` — snapshot every registered repo as **one bundled manifest**. Rollback affects all of them together. Use for cross-cutting refactors / suite-wide migrations.
- `--each` — create **N independent per-project snapshots** (one manifest each, but they share the friendly name). Use for "daily checkpoint across the portfolio." Each project can be rolled back independently. Implies `--all` if no `--only` is given.
- `--only sapptools,sappcode` — snapshot a named subset of registered repos. Combines with `--each`.
- `--no-push` — local-only (don't push tags to origin).
- A friendly name string (positional) — e.g. `before-firebase-compat-switch` or `daily-2026-05-17`.

**Picking the right mode:**

| If the user wants… | Use |
|---|---|
| Bookmark this project before a risky change | (no flags) |
| Bookmark all 30 projects in lockstep for a cross-cutting refactor | `--all` |
| Daily/weekly checkpoint for every project (each rolls back independently) | `--each` |
| Same as `--each` but only some projects | `--each --only NAME1,NAME2` |

Use this before any risky operation in the current project. The snapshot is non-destructive and survives a Mac crash because the tag pushes to the project's origin (and the manifest is pushed to sapptools/origin).

If `sapp-snapshot` is not on PATH, tell the user to run the install step from `~/apps/sapptools/_project/SNAPSHOTS.md` and stop.
