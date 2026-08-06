# refactor/ — PR-per-finding flow

The flow is a procedure, not a script: PLAYBOOK.md → **refactor** (phase B)
defines it. Summary:

- Input: `findings/F-*.md` with `refactor: true`.
- Per finding: branch `refactor/F-NNNN-<slug>`, apply only `suggested_fix`,
  host `./verify.sh` must pass, PR titled `[F-NNNN] <summary>` linking the
  finding file.
- Batched mode (one PR for all) is opt-in via `config/ship-rules.yml`
  → `refactor_mode`.
- Findings are never marked fixed here — the next audit verifies and
  deletes them.
