---
description: Print the latest sapptest verdict.
---

Print `_test/verdict/latest.md` to the user. Don't interpret — just
print.

## Steps

1. If `_test/verdict/latest.md` is missing, say:
   *"No sapptest verdict found. Run `/audit` to produce one, or
   `/update-sappcode` if `_test/` itself is missing."*

2. Otherwise, print the file contents.

3. **Staleness check:** compare the verdict's `audit_date` (from
   `_test/verdict/latest.json`) against the latest commit on the
   current branch via `git log -1 --format=%cI`. If the latest commit
   is newer than `audit_date`, append a one-line warning:
   *"⚠ Verdict is older than the most recent commit — consider running
   `/audit`."*

4. Don't run an audit yourself. Don't modify any files.
