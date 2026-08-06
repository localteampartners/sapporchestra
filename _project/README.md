# _project/ — project documentation for sapporchestra

<!-- UPDATE WHEN: a doc file is added or removed from this folder, or the reading order below becomes misleading -->

This folder contains everything you need to understand, operate, and rebuild
the project. If you're coming back cold, read the files below **in this order**.

Agent instructions live one level up in [../CLAUDE.md](../CLAUDE.md).

**Resuming mid-task?** Read [HANDOFF.md](HANDOFF.md) first — it's the
work-in-flight journal, and when work is active it outranks everything below.

---

## Read in order

| # | File | Purpose |
|---|---|---|
| 1 | [SPEC.md](SPEC.md) | **What** we're building and **why**. Goals, scope, non-goals. |
| 2 | [ROADMAP.md](ROADMAP.md) | Themed view of where this project is headed (now / next / later). |
| 3 | [ARCHITECTURE.md](ARCHITECTURE.md) | Tech stack, components, data flow, key directories. |
| 4 | [CURRENT_STATE.md](CURRENT_STATE.md) | What's built, what's deployed, what's broken *right now*. |
| 5 | [CONVENTIONS.md](CONVENTIONS.md) | How we work here — deploy style, version pins, gotchas, "never do X." |
| 6 | [INFRASTRUCTURE.md](INFRASTRUCTURE.md) | Is there a VPS? Where is it hosted? How to access it. |
| 7 | [RUNBOOK.md](RUNBOOK.md) | How to run locally, deploy, restart, roll back. |
| 8 | [ENVIRONMENT.md](ENVIRONMENT.md) | Every env var + where the real secret value lives. |
| 9 | [DEPENDENCIES.md](DEPENDENCIES.md) | External services, APIs, paid accounts. |
| 10 | [DECISIONS.md](DECISIONS.md) | Why we chose X over Y (ADR log). |
| 11 | [TODO.md](TODO.md) | Tactical task list (what's next at the item level). |
| 12 | [CHANGELOG.md](CHANGELOG.md) | Dated log of meaningful changes. |
| — | [HANDOFF.md](HANDOFF.md) | Work-in-flight journal. Read **first** when resuming; empty between tasks. |

---

## Staying current

Each file has a `<!-- UPDATE WHEN: ... -->` header that tells you when to edit
it. The routing table in [../CLAUDE.md](../CLAUDE.md) maps events (shipped a
feature, picked a library, added an env var) to the files that need updating.
Run `/sync` anytime to detect drift.
