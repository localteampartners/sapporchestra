# DECISIONS — sapporchestra

<!-- UPDATE WHEN: you make a non-obvious choice (library pick, architectural pattern, tradeoff). One entry per decision, newest at top. -->

The *why* behind choices that aren't self-evident from the code. The #1 question
future-you will ask is "why did I do it this way?" — answer it here, once, when
it's fresh.

Skip obvious decisions ("I used Express because it's a Node web framework").
Write decisions where someone smart would reasonably pick differently.

---

## Format

```
## YYYY-MM-DD — short title

**Decision:** what you chose.
**Context:** the situation that forced the choice.
**Alternatives considered:** what else was on the table, and why they lost.
**Tradeoffs:** what this choice costs you.
**Revisit if:** the condition that would make you reconsider.
```

---

## Entries

<!-- Newest first. Example below — delete once you have real entries. -->

## YYYY-MM-DD — Example: chose SQLite over Postgres

**Decision:** use SQLite with WAL mode for v1.
**Context:** single-user app, will run on one VPS, expected <1k writes/day.
**Alternatives considered:** Postgres (heavier ops for no current benefit),
DuckDB (analytical, not OLTP), plain JSON files (no concurrency safety).
**Tradeoffs:** can't scale out horizontally; migrations are manual-ish.
**Revisit if:** multi-user, multi-writer, or dataset >10GB.
