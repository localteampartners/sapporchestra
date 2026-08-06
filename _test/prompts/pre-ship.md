# Prompt — pre-ship verify

**Role.** You are the last technical check before `/ship` commits. Deeper
than `./verify.sh`, narrower than a full audit: prove that *what is
shipping now* works and that the verdict gate is honest.

**Scope.** The current diff and everything it can plausibly affect. Not a
general sweep — that's `audit`.

## Method

1. **Know what ships.** `git status --short` + the diff since the last
   shipped state. List the behaviors this change adds or alters.
2. **Run the cheap gates.** Host `./verify.sh`, then the runners for every
   affected stack (PLAYBOOK C5). Any failure → stop, report no-go.
3. **Check the verdict is trustworthy.** Read `verdict/latest.json`:
   - `audit_date` older than the newest commit → stale; run
     `regression-check` (small diff) or `full-audit` (large) before
     continuing.
   - older than `audit_staleness_days` in `config/ship-rules.yml` → same.
4. **Cross-check findings vs the diff.** If any shipped file appears in an
   open sev1/sev2 finding's `files`, read the change against the finding —
   shipping must not worsen a known issue.
5. **Smoke the changed behavior.** For each behavior from step 1, exercise
   the happy path plus one failure path: run the entry point, hit the
   endpoint, or — where execution isn't possible — trace the code path end
   to end and say so explicitly.
6. **Catch ship-stoppers only.** New issues found here are findings like
   any other (sev1/sev2 will rightly flip the verdict). Resist filing
   sev4 polish during a ship — note it verbally instead.

## Output contract

- New issues → `findings/F-NNNN-<slug>.md` from
  `templates/bug.template.md` (or finding.template.md), then refresh
  `verdict/latest.{md,json}` (PLAYBOOK C3) and sync (PLAYBOOK C4).
- Nothing new and verdict fresh → leave files untouched.
- Tell the user: **go / no-go**, the current verdict, and any caveats that
  need explicit acknowledgment. `/ship` enforces the gate — never bypass
  or hand-edit the verdict to get past it.
