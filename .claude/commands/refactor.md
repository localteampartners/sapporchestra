---
description: Open one PR per refactor-marked sapptest finding.
---

Run the `refactor` procedure from `_test/PLAYBOOK.md`. Default mode is
one PR per finding; batched mode is opt-in via `_test/config/ship-rules.yml`.

## Steps

1. **Verify sapptest is installed.** If `_test/findings/` is missing or
   empty, abort and tell the user there's nothing to refactor.

2. **Check the playbook.** If `_test/PLAYBOOK.md` is the scaffold
   (status: scaffold), tell the user sapptest's refactor procedure
   isn't published yet and ask which finding(s) to fix manually.

3. **Read the refactor mode.** From `_test/config/ship-rules.yml`,
   read `refactor_mode` (`per-finding` default, `batched` opt-in).

4. **Filter findings.** Read `_test/findings/F-*.md`. Filter to those
   marked `refactor: true` in their front matter. If none, say so and
   exit.

5. **Per-finding mode (default):** for each finding `F-NNNN`:
   - Branch: `git switch -c refactor/F-NNNN-<slug>`
   - Apply the suggested fix from the finding file. **Don't improvise
     beyond what the finding describes.**
   - Run `./verify.sh`. If it fails, abort that finding's PR.
   - Stage, commit. Push the branch.
   - Open a PR titled `[F-NNNN] <summary>` with body that links back
     to `_test/findings/F-NNNN-<slug>.md`.
   - Switch back to the original branch before processing the next
     finding.

6. **Batched mode:** one branch, one commit, one PR — body lists every
   `F-NNNN` referenced.

7. **Don't mark findings fixed.** The next `/audit` verifies and closes
   them. The finding file stays as-is until then.

## Hard rules

- **No direct commits to the working branch.** Always use a
  `refactor/...` branch + PR.
- **Don't merge PRs yourself.** Every refactor PR needs human review.
- **One finding per PR** unless `refactor_mode: batched` is set.
- **Run `./verify.sh` per PR.** If verify fails, abort the PR — don't
  push broken code.
