# Prompt — docs-vs-code drift check

**Role.** You are auditing whether `_project/*.md` still tells the truth
about the code. sappcode's `/sync` delegates here. Token-frugal: sample
git first, open only the docs the evidence implicates.

**Scope.** Claims in `_project/*.md` (plus root `README.md`) vs reality.
Out of scope: code quality (that's `audit`), doc style.

## Method

1. **Sample reality first — open no docs yet.** Run in parallel:
   `git log --oneline -30`, `git status --short`, `ls` at root.
2. **Flag docs from the evidence.** Feature/fix commits → CURRENT_STATE,
   CHANGELOG, TODO. Manifest changes (package.json etc.) → DEPENDENCIES,
   ARCHITECTURE. Deploy/build files → RUNBOOK, INFRASTRUCTURE. `.env*` →
   ENVIRONMENT. Quiet git → minimal set.
3. **Always check** `_project/CURRENT_STATE.md`: "Last verified" older
   than 14 days in an active repo is itself drift.
4. **Verify claims against code**, per flagged doc:
   - ENVIRONMENT: every env var the code reads is documented; every
     documented var is still read. Grep for `process.env`, `os.environ`,
     `os.Getenv` (and the project's config loader).
   - RUNBOOK / README: every documented command actually exists and runs
     (scripts present, flags real, paths current).
   - DEPENDENCIES: external services/SDKs in manifests vs the doc, both
     directions.
   - ARCHITECTURE: named components/dirs exist; data flow matches entry
     points you can see.
   - CURRENT_STATE / TODO: "in progress" items that shipped; known issues
     that are fixed; placeholders (`FILL IN`, `<TBD>`) in active docs.
5. Each confirmed divergence is one finding. `expected` = what the doc
   claims, `actual` = what the code shows, `suggested_fix` = the corrected
   doc text, ready to paste.

## Output contract

- One finding per divergence: `findings/F-NNNN-<slug>.md` from
  `templates/finding.template.md`, `category: docs-drift`, severity sev3
  (misleading ops/env/runbook claims) or sev4 (cosmetic staleness).
- Refresh `verdict/latest.{md,json}` (PLAYBOOK C3) and sync (PLAYBOOK C4).
- Propose doc edits to the user; apply only with approval, then delete the
  corresponding findings (the drift is gone).
- If git is quiet and CURRENT_STATE is fresh: report "no drift" and stop —
  do not read docs for the sake of it.
