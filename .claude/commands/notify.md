---
description: Send a one-line update to this project's sappchatbot Matrix room.
---

Send a one-line update to this project's sappchatbot Matrix room.

Usage: `/notify <text>`

Run sappchatbot via whichever of these resolves first:
  1. `sappchatbot send "<text>"`  — if it's on PATH (npm linked).
  2. `node "${SAPPCHATBOT_DIR:-$HOME/tools/sappchatbot}/dist/cli/index.js" send "<text>"`
     — if the local repo exists at $SAPPCHATBOT_DIR.
  3. `npx --yes sappchatbot send "<text>"` — once sappchatbot is published to npm.

Override the local-repo location by exporting `SAPPCHATBOT_DIR` before
running Claude Code (default: `~/tools/sappchatbot`).

If `.sappchatbot/config.json` doesn't exist, tell the user to run
`sappchatbot init --hooks` first (resolved via the same chain above).
