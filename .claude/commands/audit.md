---
description: Run a sapptest full audit — produces findings, verdict, and TODO sync.
---

Run the `full-audit` procedure from `_test/PLAYBOOK.md`. The audit must
never edit application code; it only writes to `_test/` and to tagged
lines in `_project/TODO.md` + `_project/CURRENT_STATE.md`.

## Steps

1. **Verify sapptest is installed.** If `_test/` is missing, abort and
   tell the user to run `/update-sappcode`.

2. **Check the playbook is real.** Open `_test/PLAYBOOK.md`. If the
   first lines say "Status: scaffold" / "Pending sapptest release", the
   real playbook hasn't been delivered yet. Stop and tell the user
   sapptest hasn't shipped a usable playbook; ask whether to proceed
   manually or wait.

3. **Open the named prompt.** The `full-audit` procedure references a
   specific prompt under `_test/prompts/`. Open it and follow it
   verbatim.

4. **Write findings.** Use the template in `_test/templates/` and the
   schema in `_test/schemas/`. One file per finding:
   `_test/findings/F-NNNN-<slug>.md`. **Validate every finding against
   the schema** — fix output, never disable validation.

5. **Compute the verdict.** Apply rules from
   `_test/config/ship-rules.yml` against open findings. Write to
   `_test/verdict/latest.md` (human-readable) and
   `_test/verdict/latest.json` (machine-readable).

6. **Sync into _project/.** For each new finding, upsert a tagged line
   into `_project/TODO.md` (`<!-- F-NNNN -->`) at the priority defined
   in `_test/config/severity.yml`. Also update
   `_project/CURRENT_STATE.md`'s "Known broken / flaky" section for
   sev1/sev2.

7. **Append a report.** Date-stamped audit summary at
   `_test/reports/YYYY-MM-DD-<branch>.md`.

8. **Don't commit.** Show what changed. Let the user review.

## Hard rules

- **Never edit application code during an audit.**
- **Don't read `_test/findings/` to decide ship/no-ship** — that's
  `/ship`'s job via `ship-rules.yml`.
- **User-added TODO items** (no `<!-- F-NNNN -->` tag) are never
  modified, moved, or removed by the sync.
- **If schema validation fails**, fix the output. Don't disable the
  schema.
