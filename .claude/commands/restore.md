---
description: Restore this project's artifacts from Backblaze B2 backups via sappbackup, with safety gates. Pulls into a sandbox directory by default — never overwrites live state without explicit confirmation.
---

This command orchestrates project recovery using the `sappbackup` CLI primitive (`sappbackup restore`) plus optional secret-restoration via `sappvault`. Every destructive step has a confirmation gate. Default destination is a sandbox directory; promoting to live is a separate explicit step.

If the project does not have `.sappbackup.yml`, this plugin isn't installed. Tell the user to enable it (`--with-backup` or `plugins.sappbackup.enabled: true`) and stop.

## Recovery flow

### 1. Discover what's available

Read `.sappbackup.yml` to learn the project_id and configured artifacts. Then:

```bash
# Show what restore points exist in the hot tier (latest first)
sappbackup restore "$(awk '/^project_id:/ {print $2}' .sappbackup.yml)" --dry-run --tier hot 2>&1 | head -20 || true
```

If `--dry-run` isn't supported on `restore`, run `sappbackup keys` to confirm B2 creds are present, then proceed. Surface to the user: project_id, tier, and the date range visible.

### 2. Confirm scope with the user

Ask, plainly, three things:

1. **Which restore point?** Default = latest. If they want a specific date: `--at YYYY-MM-DD`.
2. **Which artifacts?** All (default), or a subset (`code`, `docs`, `data`, etc. matching `.sappbackup.yml` artifact names).
3. **Where?** Default = `./restore-<project>-<date>/` sandbox. NEVER suggest restoring directly over `./` without an explicit user opt-in.

Don't proceed until all three are answered (latest+all+sandbox is a fine default; just confirm once).

### 3. Pull the artifacts

```bash
sappbackup restore <project_id> [--at YYYY-MM-DD] [--tier hot|cold] --out <sandbox-dir>
```

`sappbackup restore` validates byte sizes and refuses to write into a non-empty dir. If it fails partway, surface the error and stop — don't try to clean up partial state.

### 4. Inventory + propose apply plan

For each file in the sandbox dir, identify the artifact type from `.sappbackup.yml` and propose how to apply it:

| Artifact type | How to apply | Risk |
|---|---|---|
| `git_tarball` | `tar -xf <file> -C <target>` (overwrites tracked files). Better: `git clone origin && git checkout <sha-at-backup-time>` if the sha is known. | High — overwrites code |
| `project_docs` | `tar -xf <file> -C <target>` into `_project/` | Medium — overwrites docs |
| `db_pg_dump` | `pg_restore` or `psql -f` against the target DB | **Very high — destroys current DB** |
| `db_sqlite` | Copy the file over the live DB after stopping the service | **Very high — destroys current DB** |
| `path_tarball` | `tar -xf <file> -C <source-path>` | Variable |

**Print the plan as a checklist, do not execute it.** The user reviews and approves item-by-item.

### 5. Apply approved items

Only the items the user said yes to. Each apply step:

- For DB restore: confirm the service is stopped first; back up the current DB to a `.before-restore` copy; THEN apply.
- For code: prefer `git clone + checkout <sha>` over the tarball when possible. The tarball is a fallback for when GitHub is unavailable.
- For docs / paths: extract over the target.
- For each successful step: print `✓ applied: <thing>`.
- For any failure: stop immediately, do not continue with subsequent items.

### 6. Verify

Run the project's healthcheck if available:

```bash
# From .monitor.yml's primary URL
curl -sf "$(awk '/primary:/ {print $2; exit}' .monitor.yml)" >/dev/null && echo "✓ live healthcheck" || echo "⚠ healthcheck failed"
```

Plus any project-specific verification from `_project/RUNBOOK.md`. If verify.sh exists and applies to the restored tree, run it.

### 7. Record the restore in _project/BACKUP.md

Append a line to the "Restore tests" section:

```
- 2026-MM-DD: restored <artifacts> from <tier>/<date> to <where>, OK [or describe failure]
```

This is the audit trail. Even a sandbox dry-run-style restore counts — it proves the backup is restorable.

## Rules

- **Sandbox by default.** Never overwrite live state without an explicit user opt-in for THIS run. "Once" is not authorization for "every time."
- **Confirm per artifact, not in bulk.** Different artifact types have wildly different risk. A `project_docs` restore is low-risk; a `db_pg_dump` over a live DB is irreversible. Don't batch the confirmation.
- **DB restore stops the service first.** Always. If the user says "do it without stopping," push back once, then surface the risk and stop if they're not present.
- **Code restore prefers git.** A tarball is correct bytes-at-the-time but loses git history. Use `git clone + checkout` when origin is reachable.
- **Failure stops the chain.** Don't attempt step N+1 if step N failed. The user reviews and decides.
- **No silent secret writes.** If `sappvault` is needed for env restoration, run it as a separate, named step the user has approved. Never paste secret values into files unless the user explicitly said yes.

## Wiring into a cadence

`/loop monthly /restore <project> --dry-run` exercises the restore path without applying, so you find out backups are broken *before* you need them. Skip this if you don't have a sandbox machine to restore into.
