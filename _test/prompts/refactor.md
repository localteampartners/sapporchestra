# Prompt — refactor proposal

**Role.** You are a maintainer proposing surgical refactors with concrete
payoff. You are not redesigning the system and you are not chasing taste.

**Scope.** Code structure: duplication, dead code, complexity hot spots,
misplaced responsibilities, awkward internal APIs. Out of scope: behavior
changes, dependency swaps, formatting, renames without payoff.

## Method

1. Find candidates with evidence, not vibes:
   - duplication: the same logic in 2+ places (count them)
   - dead code: unexported + uncalled, feature-flagged off forever
   - hot spots: functions where `git log` shows repeated bug fixes
   - complexity: deep nesting, boolean-flag parameters, god functions
2. For each candidate, demand a payoff you can state in one sentence
   (fewer bug surfaces, one place to change X, N lines deleted). No
   stateable payoff → drop it.
3. Verify the refactor is behavior-preserving by reading every call site.
   If existing tests would not catch a slip, say which test to add first
   in `suggested_fix`.
4. Size each proposal to one reviewable PR. Too big → split into multiple
   findings, ordered, each independently shippable.
5. Severity: usually sev3; sev4 for polish; sev2 only when the structure
   actively breeds bugs (and say which bugs).

## Output contract

- One finding per proposal: `findings/F-NNNN-<slug>.md` from
  `templates/refactor.template.md`, `refactor: true`, with `branch` and
  `pr_title` filled, validated against `schemas/finding.schema.json`.
- `suggested_fix` must be applyable mechanically: name the new/changed
  symbols and which call sites move.
- Refresh `verdict/latest.{md,json}` (PLAYBOOK C3) and sync (PLAYBOOK C4).
- Proposing never touches app code. Execution is PLAYBOOK → refactor
  phase B: one PR per finding, `./verify.sh` green before each PR.
