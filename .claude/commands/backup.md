---
description: Verify this project's backups using the playbooks in _backup/. No CLI, no daemon — Claude reads the playbooks and runs the checks.
---

This command drives the verification playbooks in [`_backup/`](../_backup/) —
prose you read and follow, not a script.

**There IS a `sappbackup` CLI** (`~/tools/sappbackup`, on PATH). It does the
*doing*: builds artifacts, pushes them to Backblaze B2 and the NAS, and
`sappbackup drill` restore-verifies them. The playbooks here do the *judging*:
is coverage complete, is anything unverified. Use both — `sappbackup status`
and `sappbackup drill --project <id>` are the fastest evidence for several
playbook steps, and far better than asking the user to confirm from memory.

**If this project is a website** (anything under `~/web/`), read
`~/tools/sappbackup/_project/SITES.md` FIRST. It is the authoritative coverage
matrix and it will tell you, per site, whether the database is actually backed
up — which is the thing most likely to be missing and the thing a repo-level
check cannot see. A site's git repo is not the site.

If the project has no `_backup/` folder, the playbooks were never scaffolded.
That does **not** mean the project is unbacked — check `sappbackup status` and
the project's `.sappbackup.yml` before telling the user anything. Several live
client sites are fully backed up with no `_backup/` folder at all. Offer to
enable the plugin (`--with-backup`), but report the real coverage either way.

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
