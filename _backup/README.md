# `_backup/` — verification playbooks

This folder holds the markdown playbooks the `/backup` slash command
follows when Claude verifies that sapporchestra's assets are
redundantly backed up.

The playbooks are prose, not code. Reading them as a human is fine; the
intended consumer is Claude, who runs the verification steps and writes
findings to `_project/TODO.md` (tag: `[backup]`).

## Verification vs execution

This folder verifies; it does not execute. The actual backup push is
done by the [`sappbackup`](https://github.com/localteampartners/sappbackup)
CLI, which uploads each project's code, databases, `_project/` docs, and
runtime artifacts into the three Backblaze B2 buckets (`sapplab-hot`,
`sapplab-cold`, `sapplab-media`).

| Tool | Role |
|---|---|
| `sappbackup` CLI | **Executes** backups — `sappbackup push <project>`, `sappbackup push --all`, `sappbackup restore <project>`. |
| This plugin (`_backup/` playbooks) | **Verifies** the result — Claude reads the playbooks and confirms each backup category actually landed somewhere recoverable. |

Typical loop: the CLI runs on a schedule (cron, systemd timer, or your
host of choice); `/backup` runs on-demand or on `/loop weekly` and
flags gaps in `_project/TODO.md`. See `sappbackup --help` for command
details.

## Files

| Playbook | Covers |
|---|---|
| [`verify.md`](verify.md) | Top-level driver — runs all area playbooks and aggregates findings |
| [`source-code.md`](source-code.md) | Git remotes, push state, GitHub redundancy |
| [`env-and-secrets.md`](env-and-secrets.md) | Env var inventory, secret-store presence, no leaks in git |
| [`vps-config.md`](vps-config.md) | Customized `/etc/`, systemd units, off-host config copy |
| [`deployed-data.md`](deployed-data.md) | DBs, uploads, bucket data — replication + restore tests |

## How they're used

- `/backup` (no args) — Claude reads `verify.md`, runs all four area
  playbooks, updates `_project/BACKUP.md` status table, writes gaps to
  `_project/TODO.md`.
- `/backup verify <area>` — runs one playbook (`source-code`,
  `env-and-secrets`, `vps-config`, `deployed-data`).
- `/backup gaps` — lists current `[backup]` items in TODO without
  re-running anything.
- `/backup status` — quick read of the BACKUP.md status table.

## On-demand vs scheduled

sappbackup has no daemon. To run verification on a cadence, wire it
into `/loop`:

```
/loop weekly /backup
```

That's it. The system stays prose-driven; Claude does the work.

## Tailoring playbooks

Each playbook is yours to edit per project. If sapporchestra has no
VPS, delete `vps-config.md`. If there's no deployed data (static site
served from a CDN), delete `deployed-data.md`. The driver `verify.md`
runs whatever playbooks are present — no config to update.

When you delete a playbook, also remove its row from the status table in
`_project/BACKUP.md`.
