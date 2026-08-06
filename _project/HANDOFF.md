# HANDOFF — sapporchestra

<!-- UPDATE WHEN: starting a multi-step task (write the plan BEFORE the first edit), after each completed step (tick it), the moment a gotcha is hit or an approach is ruled out, and when the work ships (reset this file — durable facts graduate to CURRENT_STATE.md / CHANGELOG.md per the routing table) -->

**Work in flight:** none <!-- or: yes — <one line>. Keep this accurate; /resume reads it first. -->
**Last updated:** <!-- YYYY-MM-DD HH:MM -->

The session-continuity journal. If a session dies, compacts, or moves to
another machine, this file **is** the working context — a brand-new session
must be able to resume from it alone, without the old transcript. Update it
*while* working, not after.

---

## Active task

<!-- One or two lines: what we're building/fixing and why. -->

## Plan

<!-- Checklist written before starting. Tick items as they complete. -->

- [ ]

## Done so far

<!-- What's actually finished: files touched, and crucially what's DEPLOYED vs local-only / uncommitted. -->

-

## Discovered along the way

<!-- The expensive-to-relearn stuff: gotchas hit, dead ends already ruled out, constraints found mid-task. A new session must not re-pay for these. Durable workflow rules also go to CONVENTIONS.md. -->

-

## Next step

<!-- Exactly where to pick up — specific enough to act on immediately. -->

-

---

When the task ships: move durable facts into CURRENT_STATE.md / CHANGELOG.md,
then reset this file to `Work in flight: none` with empty sections. Never let
a stale handoff masquerade as live work — if it contradicts `git log`, say so.
