---
description: Process pending follow-up instructions from the sappchatbot Matrix inbox.
---

Read `.sappchatbot/inbox.md`. Each line is a follow-up instruction sent
from a Matrix user via sappchatbot.

For each line, in order:

1. Treat it as a user instruction in the context of this project.
2. Act on it. If you can't proceed, post a clarifying question in the
   room via sappchatbot (see resolution chain below).
3. Move the handled line to `.sappchatbot/inbox.archive.md` prefixed
   with `[handled YYYY-MM-DD HH:MM]`, and remove it from
   `.sappchatbot/inbox.md`.

If `.sappchatbot/inbox.md` is missing or empty, say "no pending messages"
and stop.

When done, post a "handled N message(s) from inbox" notice via
sappchatbot so the user knows their messages were picked up.

## Running sappchatbot

Use whichever of these resolves first:
  1. `sappchatbot send "<text>"`  — if it's on PATH (npm linked).
  2. `node "${SAPPCHATBOT_DIR:-$HOME/tools/sappchatbot}/dist/cli/index.js" send "<text>"`
     — if the local repo exists at $SAPPCHATBOT_DIR.
  3. `npx --yes sappchatbot send "<text>"` — once sappchatbot is published to npm.

Override the local-repo location by exporting `SAPPCHATBOT_DIR` before
running Claude Code (default: `~/tools/sappchatbot`).
