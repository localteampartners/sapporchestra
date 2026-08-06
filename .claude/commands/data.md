---
description: Drive sappdata for this project — wire the data: block, run DB-complete backups, restore drills, and DB-health checks. Creds stay in sappvault; the agent never sees them.
---

This command drives the `sappdata` CLI for the current project. sappdata is
the suite's data layer: it produces database-complete backup bundles (handed
to sappbackup for storage), restores them to an ephemeral DB to prove they
work, and posts DB-health signals to the monitor.

If `sappdata` is not on PATH, this plugin's tool isn't installed. Tell the
user to install it and stop:
```
git clone https://github.com/localteampartners/sappdata.git ~/tools/sappdata
cd ~/tools/sappdata && npm install && npm run build && npm link
```

The project is resolved by directory — run these from the project root, or
pass `.` as the project argument.

## Subcommands

Dispatch on the first word after `/data`:

### `/data` or `/data status` — where does this project stand?

1. Check whether `sapp.yml` has a `data:` block (`grep -A8 '^data:' sapp.yml`).
   - **No block:** the project may have no database, or it hasn't been wired.
     Run `sappdata wire . --apply` to auto-detect and write one (it no-ops if
     there's no supported database). Report what it did.
   - **Has a block:** read it back and report provider + what's included.
2. Run `sappdata health . --json` and summarize: size, RLS on/off, PITR
   window, slow queries, bloat. Flag anything `warn`/`fail`.
3. Tell the user whether DB creds are present (a backup will fail without
   them) — see "Credentials" below.

### `/data wire` — (re)detect and write the data: block

Run `sappdata wire . --apply`. It auto-detects Supabase / Postgres / SQLite,
writes a `data:` block to `sapp.yml` if one is missing, and never clobbers an
existing block or writes secrets. Report the provider and action. If the
provider needs creds, point the user at the Credentials section.

### `/data backup` — produce a DB-complete bundle

Run `sappdata backup .`. This streams schema + data (+ Storage/Auth/RLS for
Supabase) to a bundle and registers it with sappbackup for B2/NAS storage.
The data never passes through the chat. Report the bundle summary. Add
`--dry-run` to rehearse without pushing.

### `/data drill` — prove the latest backup restores

Run `sappdata drill .`. Restores the newest bundle to an ephemeral database,
checks integrity + row-count sanity, posts a `drill` status event. Report
PASS/FAIL. An untested backup is a hope — run this after wiring creds.

### `/data health` — probe and post signals

Run `sappdata health .` (add `--json` for machine output). Posts ≥5 signals
to the monitor; RLS-off and near-PITR-expiry surface as warn/fail.

### `/data schedule` — automate it

`sappdata schedule install . --load` installs this project's daily backup +
weekly drill. (For the whole portfolio at once, the suite runs
`sappdata schedule install --all --load` from the umbrella — you usually
don't need per-project schedules.)

## Credentials (sappvault — never plaintext)

Supabase/Postgres backups need creds in this project's vault. The agent must
NOT ask for or handle the values; tell the user to run these themselves:
```
pbpaste | sappvault set <project> DATABASE_URL --from-file -
pbpaste | sappvault set <project> SUPABASE_SERVICE_ROLE_KEY --from-file -   # Supabase only
```
The `supabase_url` / `project_ref` / sqlite `file` in `sapp.yml` are NOT
secrets and are fine to commit. Only `DATABASE_URL` and the service-role key
live in the vault.

## Keep the docs current

After a meaningful change (wired, scheduled, first successful drill), update
the status table in [`_project/DATA.md`](../../_project/DATA.md) — edit that
table only, don't rewrite the file.
