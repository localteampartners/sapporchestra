# Backups — sapporchestra

<!-- UPDATE WHEN: a `/backup` run completes (last_verified date), gaps
     are discovered or closed, the off-host destination changes, or a
     restore is performed (record date + outcome). -->

This is the **status doc** for backups. The verification logic lives in
[`_backup/`](../_backup/) — markdown playbooks Claude reads when you run
`/backup`. This file records *what's currently true*, not how to check.

## Definition of "backed up"

For each asset class, "backed up" means **at least 2 redundant copies,
at least 1 of them off-host**:

- **Source code** — GitHub remote (origin) + at least one of: a backup
  remote, a local clone on a separate machine, or a documented
  acceptance that GitHub-only is enough for this project's risk
  tolerance (record in `_project/DECISIONS.md`).
- **Env / secrets** — your password manager / secret store + at least
  one other restorable location.
- **VPS config** — your provisioning sources (if any) + an off-host
  backup of customized paths (`/etc/nginx/`, `/etc/systemd/system/`,
  customized scripts).
- **Deployed data** — production data on the VPS + replicated to an
  off-host destination on a regular cadence, with at least one
  successful restore on record.

## Status

<!-- /backup updates this block. Treat as machine-touched. -->

| Area | Status | Last verified |
|---|---|---|
| Source code | <!-- ok | gaps | unverified --> unverified | <!-- date --> never |
| Env / secrets | unverified | never |
| VPS config | unverified | never |
| Deployed data | unverified | never |

## Known gaps

Tagged `[backup]` in `_project/TODO.md`. To list them:

```bash
grep -n '\[backup\]' _project/TODO.md
```

## Restore tests

A backup you've never restored from is a backup that doesn't work.
Record each successful restore here with the date and what was restored.

<!-- - YYYY-MM-DD: restored deployed data from <destination> to /tmp/restore-test, OK -->

## How to verify

In Claude Code: `/backup` (full run) or `/backup verify <area>` for one
area. The playbooks in `_backup/` tell Claude what to check; the slash
command updates this status table and writes any new gaps to
`_project/TODO.md`.

## How to recover

In Claude Code: `/restore` walks the recovery flow.

1. Discovers available restore points in B2 (latest + `--at YYYY-MM-DD`
   for a specific date).
2. Asks which artifacts (`code`, `docs`, `data`, etc.) and where to
   restore to (sandbox dir by default — never overwrites live state
   without explicit confirmation).
3. Calls `sappbackup restore <project> --out <sandbox>`.
4. Proposes a per-artifact apply plan with risk annotations
   (code = high, DB = very high, docs = medium).
5. Applies only what you approve, one at a time. DB restores stop the
   service and snapshot the current DB to `.before-restore` first.
6. Verifies via the project's healthcheck (`.monitor.yml`).
7. Records the run in "Restore tests" above so you have proof the
   backup is actually restorable.

Even a dry-run sandbox restore is worth doing on a cadence
(`/loop monthly /restore <project> --dry-run`) — a backup that's
never been restored isn't a backup, it's hope.
