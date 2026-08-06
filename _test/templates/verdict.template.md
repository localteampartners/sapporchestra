# Verdict — <one-line summary>

**Status:** `SHIP | SHIP WITH CAVEATS | BLOCK`
**Reason:** `<reason code — see schemas/verdict.schema.json>`
**Audit date:** YYYY-MM-DD
**sapptest version:** `<contents of VERSION>`
**Open findings:** N (sev1: N · sev2: N · sev3: N · sev4: N)

<one short paragraph: which ship-rules.yml rule fired and what it takes to
reach SHIP. For BLOCK, list each blocking finding below.>

## Blocking findings

<delete this section unless verdict is BLOCK>

- `F-NNNN` (sevN) — <title> — `findings/F-NNNN-<slug>.md`

---

Write the machine-readable copy to `verdict/latest.json` (must validate
against `schemas/verdict.schema.json`):

```json
{
  "verdict": "...",
  "reason": "...",
  "audit_date": "YYYY-MM-DD",
  "sapptest_version": "...",
  "findings_open": 0,
  "sev_counts": { "sev1": 0, "sev2": 0, "sev3": 0, "sev4": 0 },
  "blocking": [],
  "notes": "..."
}
```
