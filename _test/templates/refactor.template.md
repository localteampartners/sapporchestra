---
# Refactor-proposal variant of finding.template.md. Same schema (finding.schema.json).
id: F-NNNN
id_key: <primary file>|<title-slug>
title: <the smell, concretely: "duplicate retry logic in three clients">
severity: sev3            # refactors are rarely sev1/sev2 unless they hide active bugs
status: open
category: refactor
found_date: YYYY-MM-DD
files:
  - <repo-relative/path/primary-file>
  - <every other file the fix touches>
refactor: true
repro: |
  <where to look: the duplication / complexity / dead code, with file:line refs>
expected: |
  <the better shape: one helper, clearer boundary, removed layer>
actual: |
  <current shape and what it costs: duplication count, bug surface, read difficulty>
suggested_fix: |
  <step-by-step change plan precise enough to apply mechanically;
  name the new/changed symbols and which call sites move>
branch: refactor/F-NNNN-<slug>
pr_title: "[F-NNNN] <summary>"
---

## Notes

<optional: risk assessment, suggested review order. Delete if empty.>
