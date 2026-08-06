---
# Confirmed-bug variant of finding.template.md. Same schema (finding.schema.json).
id: F-NNNN
id_key: <primary file>|<title-slug>
title: <symptom, not cause: "login rejects valid JWT", not "jwt lib misused">
severity: sev2            # bugs that corrupt data / break auth / lose work → sev1
status: open
category: bug
found_date: YYYY-MM-DD
files:
  - <repo-relative/path/where-root-cause-lives>
refactor: false
repro: |
  1. <exact command / request / click path>
  2. <input that triggers it>
  3. <observe: exact error or wrong behavior>
expected: |
  <correct behavior, citing spec/docs if they define it>
actual: |
  <observed behavior + root cause: file:line and the faulty logic in one sentence>
suggested_fix: |
  <minimal correct fix at the root cause; mention any test that should pin it>
---

## Notes

<optional: how it was found, what was ruled out. Delete if empty.>
