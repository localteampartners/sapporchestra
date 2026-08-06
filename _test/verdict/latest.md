# Verdict — no audit run yet

**Status:** `BLOCK`
**Reason:** `no_audit_yet`
**sapptest version:** `0.1.0`

No audit has been run against this project. `/ship` will refuse to proceed
until either:

- A real verdict is produced via `/audit` (or the `full-audit` procedure
  in [`../PLAYBOOK.md`](../PLAYBOOK.md)), **or**
- The user explicitly overrides via `/ship --no-audit`.

Once the first audit completes, this file is replaced.
