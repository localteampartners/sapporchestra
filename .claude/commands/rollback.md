---
description: Roll back to a named or timestamped snapshot. Destructive — confirm with the user first.
---

This is a destructive operation. Before running:

1. Confirm with the user which snapshot they want to roll back to. If they said only "roll back" without specifying, run `/snapshots` first and ask which one. Each snapshot has a manifest that knows which repos it covers — rollback only affects those repos. For `--each`-created snapshots that share a friendly name, the rollback resolver picks the manifest covering the cwd's project automatically (so `/rollback "daily"` from inside mctvnews180 restores only mctvnews180).
2. Confirm the user is aware that any commits made after the snapshot will move to the reflog (recoverable for 90 days but unreachable from `main`).
3. Confirm the user has no uncommitted local work they care about. If they do, suggest a fresh `/snapshot` first so the in-progress state is recoverable.

Once confirmed, run `sapp-snapshot rollback $ARGUMENTS --yes` and report the result. If the CLI refuses because of uncommitted changes, **do not** automatically pass `--force` — ask the user explicitly.

If `sapp-snapshot` is not on PATH, tell the user to install per `~/apps/sapptools/_project/SNAPSHOTS.md` and stop.
