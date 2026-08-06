---
description: Verify this project's backups using the playbooks in _backup/. No CLI, no daemon — Claude reads the playbooks and runs the checks.
---

This command drives the verification playbooks in [`_backup/`](../_backup/).
There is no `sappbackup` binary — the playbooks are prose, you read them
and follow the steps.

If the project does not have a `_backup/` folder, this plugin isn't
installed. Tell the user to enable the sappbackup plugin (`--with-backup`
or `plugins.sappbackup.enabled: true` in their spec) and stop.

## Subcommands

If the user typed `/backup` plus an argument, dispatch by the first word:

### `/backup` — full verification

1. Open `_backup/verify.md` and follow the steps it lays out. That
   playbook tells you to run all four area playbooks and aggregate.
2. For each area playbook present in `_backup/`:
   - Open it, follow its verification steps
   - Classify the area as `ok`, `gaps`, or `unverified`
   - Note specific gaps in plain prose
3. Update the status table in `_project/BACKUP.md` (status + last-verified
   date for each row). Edit only that table — do not rewrite the file.
4. For each new gap:
   - Check it isn't already tagged `[backup]` in `_project/TODO.md`
   - If new, append a line under TODO.md's appropriate section:
     `- [ ] [backup] <area>: <one-line gap and fix>`
5. Print a one-line summary, then up to 3 gaps inline so the user can
   triage:

   ```
   Backups: 3/4 ok, 1 gaps. Top issues:
     - source-code: 2 commits not pushed
     - env-and-secrets: no off-host secret store confirmed
   ```

### `/backup verify <area>` — one playbook only

Areas: `source-code`, `env-and-secrets`, `vps-config`, `deployed-data`.

Run the named playbook only, update its row in BACKUP.md, write its
gaps to TODO.md. Same rules as the full run, scoped.

### `/backup status` — read-only snapshot

Show the current status table from `_project/BACKUP.md` and the count
of `[backup]` items in `_project/TODO.md`. Don't run any verification.
Quick "where do I stand" check.

```bash
sed -n '/^| Area/,/^$/p' _project/BACKUP.md
echo
echo "Open backup gaps:"
grep -n '\[backup\]' _project/TODO.md | grep -v '^[^:]*:.*\[x\]' | head -10
```

### `/backup gaps` — list current gaps

Just print the `[backup]` items from `_project/TODO.md`, one per line,
with line numbers. Don't run verification.

```bash
grep -n '\[backup\]' _project/TODO.md
```

## Rules

- **You are the verifier.** Don't fabricate — if a playbook step needs
  the user to confirm something Claude can't see (e.g. "is the secret
  in your password manager?"), ask them. If they're not present
  (running via `/loop`), mark that area `unverified` with a note.
- **Don't auto-fix gaps.** This command surfaces problems and writes
  them to TODO. The user (or you, in a separate, scoped session) fixes
  them.
- **Don't auto-close gaps.** A previously-found gap that no longer
  reproduces stays in TODO until the user explicitly marks it done.
  Auto-closing risks silencing a flapping issue.
- **Don't run destructive things.** No backups are taken, no restores
  attempted, no remote state modified. Read-only verification.
- **Tagged findings.** Every gap line in TODO.md uses `[backup]` so
  `/backup gaps` (and a future `/audit` integration) can find them.

## Wiring into a cadence

`/loop weekly /backup` runs the full verification weekly. `/loop daily
/backup verify deployed-data` if data backups are the highest-risk
area. Pick a cadence that matches the project's risk tolerance.
