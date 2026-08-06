---
description: Checkpoint the current session into _project/HANDOFF.md so any future session can resume without this conversation.
---

Write the current working state into `_project/HANDOFF.md` — the file a
brand-new session (or this one, post-compaction) will read to resume. Write
for a reader who has NOT seen this conversation: no shorthand, no references
to "the fix we discussed."

Capture:

1. **Active task** — what and why, one or two lines.
2. **Plan** — the checklist, with completed items ticked.
3. **Done so far** — files touched; explicitly separate deployed vs
   local-only vs uncommitted.
4. **Discovered along the way** — gotchas hit, dead ends ruled out,
   constraints found. The most valuable section; never skip it.
5. **Next step** — the exact next action, specific enough to execute
   immediately.

Also:

- Set the top `Work in flight:` line and `Last updated:` timestamp.
- If a discovery is a durable workflow rule (not task-specific), copy it to
  `_project/CONVENTIONS.md` too.
- Overwrite the previous handoff — edit, don't append. One handoff = the
  current state, not a history.

Run this before ending a session mid-task, before anything risky, or when
the user says "checkpoint" / "write down where we are." When the task ships,
move durable facts to CURRENT_STATE / CHANGELOG (the `/ship` routing table
covers this), then reset the handoff to `Work in flight: none`.
