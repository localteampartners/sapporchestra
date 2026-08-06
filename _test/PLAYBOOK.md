# PLAYBOOK — sapptest procedures

Numbered procedures an agent follows verbatim. Shared conventions first —
every procedure references them. Audience: an LLM operator with full repo
access. All paths are relative to `_test/` unless they start with `_project/`
or `./`.

---

## Conventions (read once, used by every procedure)

### C1. Finding IDs — `F-NNNN`, stable across runs

IDs are content-hash derived so re-runs update findings instead of
duplicating them, and body-wording tweaks never change the ID.

1. **Title slug:** lowercase the finding title; replace every run of
   non-alphanumerics with `-`; trim leading/trailing `-`; cut to 40 chars.
2. **ID key:** `<primary file>|<slug>` where primary file is the first
   entry in the finding's `files` list (repo-relative path).
   Example: `src/auth/jwt.ts|login-rejects-valid-jwt`
3. **NNNN:** decimal CRC of the key, mod 10000, zero-padded to 4 digits:
   ```bash
   printf '%s' 'src/auth/jwt.ts|login-rejects-valid-jwt' | cksum | awk '{printf "%04d\n", $1 % 10000}'
   # → 7204  ⇒  id F-7204, file findings/F-7204-login-rejects-valid-jwt.md
   ```
4. **Dedup before minting.** Grep `findings/` for an existing finding with
   the same `id_key`, or any open finding on the same primary file with the
   same root cause. If found, **update that file** — never mint a second ID
   for the same issue, even if you would word the title differently today.
5. **Collision:** if the computed NNNN is already used by a finding with a
   *different* `id_key`, increment NNNN by 1 (mod 10000) until free. Keep
   `id_key` set to the original key so future dedup still matches.
6. **Filename:** `findings/F-NNNN-<slug>.md`. If a title is later reworded,
   keep the original `id` and `id_key`; ID stability beats slug accuracy.

### C2. Writing a finding

1. Copy the matching template: `templates/finding.template.md` (general),
   `templates/bug.template.md` (confirmed bug), or
   `templates/refactor.template.md` (refactor proposal).
2. Fill **every** front-matter field. Long fields (`repro`, `expected`,
   `actual`, `suggested_fix`) use YAML `|` block scalars.
3. Severity per `config/severity.yml` definitions — judge impact, not effort.
4. Validate against `schemas/finding.schema.json`: every `required` field
   present, enum values exact, `files` non-empty. Fix the file, never the
   schema.
5. A finding must be self-sufficient: an agent reading only that file can
   reproduce and fix the issue.
6. Findings are live state: when a later pass verifies an issue is fixed,
   **delete** the finding file (and its synced lines, C4). Never edit
   `reports/` history.

### C3. Computing the verdict

1. Count open findings in `findings/F-*.md` by `severity`.
2. Apply `config/ship-rules.yml` top-down (first match wins):
   - any sev1 count > `block_thresholds.sev1` → `BLOCK`, reason `sev1_open`
   - sev2 count > `block_thresholds.sev2` → `BLOCK`, reason `sev2_over_threshold`
   - sev2 count ≥ `caveat_thresholds.sev2` or sev3 count ≥
     `caveat_thresholds.sev3` → `SHIP WITH CAVEATS`, reason `open_findings_below_threshold`
   - otherwise → `SHIP`, reason `clean` (or `minor_findings_only` if any
     sev3/sev4 remain open)
3. `audit_date` = today (YYYY-MM-DD). Every procedure that examined code and
   reached this step refreshes it. Staleness (`audit_staleness_days`, and
   verdict older than the newest commit) is enforced by the *reader* —
   `/ship` — not pre-computed here.
4. `blocking` = the findings that triggered a `BLOCK` (all open sev1s; the
   sev2 overflow). Empty array otherwise.
5. Write `verdict/latest.json` (shape: `templates/verdict.example.json`,
   must validate against `schemas/verdict.schema.json`) and
   `verdict/latest.md` from `templates/verdict.template.md`. Overwrite both.

### C4. TODO / CURRENT_STATE sync (one-way, tagged, idempotent)

sapptest owns exactly the lines it tagged `<!-- F-NNNN -->`. Never touch
untagged lines; never let user edits to tagged lines survive (they are
overwritten).

1. For each open finding, upsert one line into `_project/TODO.md` under the
   section matching `todo_priority` in `config/severity.yml`
   ("Next up" / "Backlog" / "Ideas / maybe"):
   ```
   - [sev2] login rejects valid JWT — _test/findings/F-7204-login-rejects-valid-jwt.md <!-- F-7204 -->
   ```
2. Upsert = if a line containing `<!-- F-NNNN -->` exists, replace it in
   place; move it only if its section no longer matches the severity's
   `todo_priority`. Otherwise append to the right section.
3. Delete any tagged line whose `F-NNNN` no longer has a `findings/` file.
4. For open sev1/sev2 findings, also upsert into `_project/CURRENT_STATE.md`
   → "known broken / flaky" section, same line format, same tag. Remove on
   close.

### C5. Runners — normalized exit codes

`config/adapters.yml` maps detection files → runner. Run **every** adapter
that matches (mixed stacks run all). From the project root:
`bash _test/runners/run-<lang>.sh .`
Exit codes: `0` pass · `1` test failures · `2` not applicable / no tests ·
`3` environment error. Read the native output — it names the failing tests.

---

## full-audit

Non-destructive. Writes only to `findings/`, `reports/`, `verdict/`, and
tagged `_project/` lines.

1. Read `prompts/audit.md`, `config/coverage-goals.yml`,
   `config/ship-rules.yml`. Note the depth each category demands.
2. Detect stacks via `config/adapters.yml`; run each matching runner (C5).
   Record exit codes and failing test names. A failing native test is
   automatically a finding (sev2 unless impact says otherwise).
3. Map the codebase: entry points, core modules, data writes,
   `git log --oneline -30` for churn hot spots.
4. Audit per `prompts/audit.md`, applying the specialist lenses it names
   (`prompts/security.md`, `prompts/a11y.md`, `prompts/perf.md`) where
   coverage-goals require them. Confirm every issue by reading the code —
   no speculative findings.
5. For each confirmed issue: dedup (C1.4), then write a finding (C2).
6. Sweep existing `findings/`: any that the codebase shows are fixed →
   verify, then delete the file.
7. Write `reports/audit-YYYY-MM-DD.md`: scope covered vs coverage-goals,
   runner results, table of findings (ID, sev, title), closed findings,
   verdict line. Append-only — never rewrite old reports.
8. Compute the verdict (C3). Sync (C4).
9. Do not commit. Show the user what changed and the verdict.

## bug-hunt

Targeted. Input: a symptom, error message, or suspect file list.

1. Read `prompts/bug-hunt.md`. Restate the symptom as expected vs actual.
2. Trace from the symptom to candidates: entry point → handler → data.
   Read the implicated code paths fully. Run the relevant runner (C5) or a
   single native test if it shortens the hunt.
3. Confirm the root cause — name file and line. If you cannot confirm,
   report what you ruled out; write no speculative finding.
4. Write one finding per confirmed root cause using
   `templates/bug.template.md` (C2), with a minimal repro.
5. Findings changed → recompute the verdict (C3) and sync (C4).
6. Report root cause + finding ID to the user. Do not fix it in this pass —
   fixes go through `refactor` or normal feature work.

## regression-check

Input: a diff, branch, or commit range ("did this change break anything?").

1. Read the diff. List behaviors the change could plausibly affect —
   callers of changed functions, shared state, changed contracts/schemas.
2. Run runners (C5) for every affected stack.
3. For each at-risk behavior, read the post-change code and confirm it
   still holds. Check `reports/` for previously closed issues in the
   touched files — regressions of past findings get priority.
4. New breakage → findings via `templates/bug.template.md` (C2), noting the
   offending commit in `repro`.
5. Findings changed → recompute verdict (C3), sync (C4). Nothing found →
   say so; refresh the verdict only if you audited deeply enough to vouch
   for it (C3 sets `audit_date`).

## refactor

Two phases. Phase A proposes; phase B opens PRs. `/refactor` runs phase B.

**A — propose (code untouched):**

1. Read `prompts/refactor.md`. Identify improvements with concrete payoff
   (duplication, dead code, complexity hot spots, API awkwardness).
2. Write each as a finding via `templates/refactor.template.md` with
   `refactor: true` and a `suggested_fix` precise enough to apply without
   re-deriving it (C2). Recompute verdict (C3), sync (C4).

**B — execute (PR-per-finding):**

1. Read `refactor_mode` from `config/ship-rules.yml`
   (`per-finding` default, `batched` opt-in).
2. Filter `findings/F-*.md` where `refactor: true`. None → say so, stop.
3. Per finding (per-finding mode):
   a. `git switch -c refactor/F-NNNN-<slug>` from the working branch.
   b. Apply **only** the finding's `suggested_fix`. Don't improvise beyond it.
   c. Run the host project's `./verify.sh`. Fails → abort this PR, switch
      back, continue with the next finding.
   d. Commit, push, open a PR titled `[F-NNNN] <summary>`, body linking
      `_test/findings/F-NNNN-<slug>.md`. Switch back to the working branch.
4. Batched mode: one branch, one commit, one PR listing every `F-NNNN`.
5. Do **not** mark findings fixed or delete them — the next audit verifies
   merged fixes and closes them. Never merge your own PRs.

## pre-ship

Deeper gate than `./verify.sh`, run before `/ship` commits.

1. Read `prompts/pre-ship.md`. Identify what is shipping:
   `git status --short` + `git diff` against the last shipped state.
2. Run the host `./verify.sh`. Fails → stop, report, no-go.
3. Run runners (C5) for affected stacks.
4. Check `verdict/latest.json`: if `audit_date` predates the newest commit
   or exceeds `audit_staleness_days`, the verdict is stale — run
   `regression-check` (small change) or `full-audit` (large) first.
5. Cross-check the diff against open findings: if shipped files appear in
   any open sev1/sev2 finding, verify the change doesn't worsen it.
6. Smoke the changed behavior per `prompts/pre-ship.md`.
7. New issues → findings (C2), recompute verdict (C3), sync (C4).
8. Tell the user: go / no-go, the verdict, and any caveats needing
   acknowledgment. `/ship` enforces the verdict; you never bypass it.

## docs-drift

Compares `_project/*.md` claims against code. sappcode's `/sync` delegates
here. Cheap first, thorough second.

1. Read `prompts/docs-drift.md`. Sample reality before opening docs:
   `git log --oneline -30`, `git status --short`, `ls` at root.
2. Follow the prompt's claim checklist (env vars, commands, dependencies,
   architecture claims, CURRENT_STATE staleness). Open only the docs the
   git evidence implicates, plus `_project/CURRENT_STATE.md` always.
3. Each confirmed divergence → finding, `category: docs-drift`, severity
   sev3 (misleading runbook/env claims) or sev4 (cosmetic staleness), with
   `suggested_fix` containing the corrected doc text (C2).
4. Findings changed → recompute verdict (C3), sync (C4).
5. Propose the doc edits to the user. Apply them only with approval — and
   then delete the corresponding findings, since the drift is gone.
