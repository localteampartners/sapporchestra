# Playbook — deployed data

## What "backed up" means here

The mutable production data — databases, user uploads, bucket contents,
queue state, anything that's irreplaceable if the VPS dies — is
**replicated to at least 1 off-host destination on a regular cadence**,
**and at least one restore has been successfully tested**.

Two halves to that:
1. **Replication** — fresh copies land somewhere off this host.
2. **Restoration tested** — at least once, you've verified the copies
   are usable. A backup you've never restored isn't a backup.

## When this playbook does NOT apply

Stateless projects (a static site, a stateless API behind a managed
DB) have no deployed data of their own. If that's the case, delete
this playbook and the BACKUP.md row. The managed DB's vendor backups
are *their* concern; verify those separately if you care, but it's
out of scope here.

## Verification steps

1. **Inventory what data this project has.**

   Read `_project/ARCHITECTURE.md`. Look for: a database section, file
   uploads, bucket / blob storage, persistent caches.

   If ARCHITECTURE.md doesn't mention any data, double-check by
   asking the user. A surprising number of projects "don't have data"
   right up until you remember the user upload directory.

2. **Find where the data lives on the VPS.**

   Common paths:

   ```bash
   vps-proxy run "ls -lh /var/lib/postgresql/ /var/lib/mysql/ 2>/dev/null"
   vps-proxy run "df -h --output=source,target,size,used | grep -v tmpfs | head -20"
   ```

   And from RUNBOOK.md: any `data/`, `uploads/`, `storage/` paths the
   app uses.

   - Found → continue.
   - No data on disk and no buckets used → playbook doesn't apply.

3. **Backup mechanism and cadence.** (Ask the user.)

   > For each data source above, what mechanism backs it up, where, and
   > how often?

   Examples of acceptable answers:
   - Postgres: `pg_dump` cron daily, uploaded to B2 via rclone, 30-day
     retention
   - User uploads: rsynced to a second VPS nightly, restic to S3 weekly
   - Bucket data: bucket has versioning + replication enabled to a
     second region

   Record per-source. A single "backups go to S3" answer covering five
   data sources is suspicious — verify each one.

4. **Confirm freshness.** (Ask the user, then spot-check.)

   For each data source, what's the timestamp of the most recent
   successful backup?

   When possible, verify via vps-proxy:

   ```bash
   # Example for an rclone-pushed pg_dump:
   vps-proxy run "ls -lt /var/backups/postgres/ | head -3"
   # For a backup script's log:
   vps-proxy run "tail -20 /var/log/backup.log 2>/dev/null"
   ```

   - Latest backup younger than the cadence → ok.
   - Latest older than 2× cadence → hard gap (silently broken).
   - Older than the project's tolerance → soft gap.

5. **Restore test on record.**

   Read the "Restore tests" section in `_project/BACKUP.md`. At least
   one entry?

   - Yes, within the last 6 months → ok.
   - Yes, > 6 months ago → soft gap. Schedule the next one.
   - None ever → hard gap. Even one test, even on a small subset, is
     the difference between "we have backups" and "we hope we have
     backups."

6. **Restore procedure documented.**

   Read `_project/RUNBOOK.md` for a restore section: what to run, in
   what order, with what credentials, to recover the data.

   - Section exists with concrete commands → ok.
   - Vague ("restore from B2") → soft gap.
   - Missing → soft gap.

## What counts as a gap

| Severity | Condition | Example finding line |
|---|---|---|
| Hard | Data source identified with no backup mechanism | `[backup] deployed-data: <source> has no backup configured` |
| Hard | Backup older than 2× cadence (silently broken) | `[backup] deployed-data: <source> last backup <date>, expected <cadence>` |
| Hard | No restore test ever performed | `[backup] deployed-data: no restore test on record — exercise restore for <source>` |
| Soft | Restore test > 6 months old | `[backup] deployed-data: last restore test was <date> — schedule a new one` |
| Soft | Restore procedure missing or vague in RUNBOOK | `[backup] deployed-data: RUNBOOK lacks concrete restore commands` |
| Soft | Data inventory not in ARCHITECTURE.md | `[backup] deployed-data: data sources not documented in ARCHITECTURE.md` |

## What to put in BACKUP.md

- Hard gaps unresolved → **gaps**
- Hard closed, soft may remain → **ok**
- Couldn't reach the user (e.g. running via `/loop` unattended) →
  **unverified** with a note that off-host confirmation needs a human.
