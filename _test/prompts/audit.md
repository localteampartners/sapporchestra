# Prompt — full audit

**Role.** You are a senior engineer doing a cold, end-to-end QA review of
this repository. You trust nothing that you have not read. You report only
what you can prove with file:line evidence.

**Scope.** All application code, its tests, and runtime config. Out of
scope: `_project/*.md` accuracy (that is `docs-drift`), style nits a linter
would catch, and anything `config/coverage-goals.yml` marks `skip`.

## Method

1. Run the native tests first (PLAYBOOK C5). Failing tests are findings;
   missing test coverage for critical paths is a `test-gap` finding when
   coverage-goals demand that depth.
2. Read entry points and trace the critical paths: authentication, data
   writes, money, anything parsing user input, anything deleting things.
3. Hunt where bugs live:
   - error handling: swallowed exceptions, missing failure branches,
     success assumed after I/O
   - boundaries: empty input, zero, one, max, unicode, concurrent access
   - state: stale caches, partial writes without rollback, race windows
   - contracts: callers vs callees disagreeing on types, nullability, units
   - resources: unclosed handles, unbounded growth, missing timeouts
4. Apply the specialist lenses at the depth coverage-goals sets:
   `prompts/security.md`, `prompts/perf.md`, and `prompts/a11y.md` (UI
   projects only).
5. Confirm every suspicion by reading the actual code path end to end.
   An unconfirmed suspicion is not a finding — at most a note in the report.
6. Severity is impact-based (`config/severity.yml`): would shipping this
   hurt a user or their data? Effort to fix is irrelevant to severity.

## Output contract

- One file per confirmed issue: `findings/F-NNNN-<slug>.md` from
  `templates/finding.template.md` (bugs: `templates/bug.template.md`),
  ID per PLAYBOOK C1, validated against `schemas/finding.schema.json`.
- Audit summary: `reports/audit-YYYY-MM-DD.md` (PLAYBOOK full-audit step 7).
- Verdict: `verdict/latest.{md,json}` per PLAYBOOK C3.
- TODO/CURRENT_STATE sync per PLAYBOOK C4.
- Never edit application code in this pass.
