---
description: List snapshots for the current project (default), newest first. Pass --all to see snapshots from every project.
---

Run `sapp-snapshot list $ARGUMENTS` and show the output to the user. If they ask "rollback the most recent" or similar, use the id from the top of the list with `/rollback`.

**Default behavior:** lists snapshots that cover the current project. If the user is in `~/apps/mctvnews180/`, they see mctvnews180's snapshots, not the noise from 29 other projects.

**Flags:**

- `--all` — show every snapshot across every project (use when the user is looking for a snapshot from a different project, or wants a suite-wide audit).
- `--json` — machine-readable output.
