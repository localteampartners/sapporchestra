---
# Validates against schemas/finding.schema.json. ID rules: PLAYBOOK.md C1.
id: F-NNNN
id_key: <primary file>|<title-slug>
title: <one line, symptom-first, present tense>
severity: sev3            # sev1..sev4 per config/severity.yml
status: open
category: other           # bug | security | perf | a11y | docs-drift | test-gap | refactor | other
found_date: YYYY-MM-DD
files:
  - <repo-relative/path/primary-file>
refactor: false           # true = refactor procedure may auto-apply suggested_fix via PR
repro: |
  <numbered steps or code trace that demonstrates the issue>
expected: |
  <what should happen>
actual: |
  <what happens instead, with the evidence: error text, wrong value, line ref>
suggested_fix: |
  <concrete change — file, location, what to alter. Applyable without re-deriving the analysis.>
---

## Notes

<optional: alternatives considered, blast radius, links to related findings.
Delete this section if empty.>
