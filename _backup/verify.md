# Playbook — verify all areas

Top-level driver. Claude follows this when the user runs `/backup` with
no subcommand. Aim: the user has a clear answer to "is this backed up?"
in under a minute.

## Steps

1. **Inventory which playbooks exist** in `_backup/`. The full set is
   `source-code`, `env-and-secrets`, `vps-config`, `deployed-data`. If
   any are missing (the user deleted ones that don't apply), skip them
   silently.

2. **Run each present playbook in order.** For each one:
   - Open the playbook
   - Follow its steps (run commands, read files)
   - Classify the result: **ok** | **gaps** | **unverified**
   - Note specific gaps in plain prose for step 4

3. **Update the status table** in `_project/BACKUP.md`. For each row,
   set:
   - **Status**: `ok`, `gaps`, or `unverified`
   - **Last verified**: today's date in `YYYY-MM-DD`

   Edit only that table. Don't touch the rest of BACKUP.md.

4. **Write new gaps to `_project/TODO.md`** with tag `[backup]`. One
   line per gap, format:

   ```
   - [ ] [backup] <area>: <one-line description of the gap and the fix>
   ```

   Before writing, `grep '\[backup\]' _project/TODO.md` to avoid
   duplicates. If a gap already exists, leave it; if a gap was closed
   since last run, leave the existing line for the user to mark done.
   Don't auto-close items.

5. **Report a one-line summary** to the user:

   ```
   Backups: <ok-count>/<total> areas verified, <gap-count> open gaps. See _project/BACKUP.md.
   ```

   If there are gaps, list the top 3 inline so the user can see what's
   most urgent without opening another file.

## What "under a minute" means

The verification runs commands locally (git, ls, grep) and via
vps-proxy when a VPS is present. It does NOT:
- Actually back anything up
- Run restore tests
- Modify anything outside `_project/BACKUP.md` and `_project/TODO.md`
- Talk to the destinations (S3, rclone) — those checks are the user's
  domain

If a playbook needs information only the user has (e.g. "is the
deployed data replicated to off-host?"), the playbook will say so;
verify.md surfaces those as `unverified` rather than fabricating an
answer. Ask the user, or leave the gap for them to confirm.

## Wiring into a cadence

For weekly verification: `/loop weekly /backup` from the project's
Claude Code session. The user can also `/loop daily /backup verify
deployed-data` if that's the highest-risk area.
