# sync/ — TODO / CURRENT_STATE sync contract

The sync is a procedure, not a script: PLAYBOOK.md → **C4** defines it.
Summary of the contract:

- One-way: `findings/` is the source; `_project/TODO.md` and
  `_project/CURRENT_STATE.md` are targets.
- sapptest owns only lines tagged `<!-- F-NNNN -->`. Untagged (user) lines
  are never modified, moved, or removed.
- Idempotent upsert keyed by the tag; tagged lines whose finding file is
  gone are deleted. Re-running a sync on unchanged findings is a no-op.
