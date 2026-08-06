---
description: Pick up where the last session left off — reads _project/HANDOFF.md first, briefs, and continues the plan.
---

Re-orient after a dead session, a context compaction, or time away. Read in
this exact order and stop as soon as you can act — the goal is a fast, cheap
re-entry, not a full audit.

1. Read `_project/HANDOFF.md`.
   - **Work in flight** → brief the user in ≤10 lines: active task, what's
     done, what's next. Then continue the plan exactly where it stops — do
     not re-derive or redo completed steps unless something fails.
   - **No work in flight** → cold open instead: read
     `_project/CURRENT_STATE.md` and `_project/TODO.md`, then brief: what
     state the project is in and what's at the top of the task list.
     Suggest next work; don't start it unprompted.
2. Read `_project/CONVENTIONS.md` (it's short) so you don't relearn workflow
   rules mid-task.
3. Open SPEC / ARCHITECTURE / RUNBOOK only when the task at hand needs them.

Rules:

- Trust the handoff, verify cheaply: if it claims something is deployed,
  committed, or passing, a quick `git log` / `git status` glance beats
  re-running everything.
- If the handoff looks stale (old "Last updated," or it contradicts git),
  say so and ask which to trust before acting.
- In a **container**, the handoff may point at a child's own
  `_project/HANDOFF.md` — follow the pointer.
