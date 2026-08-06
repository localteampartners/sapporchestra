# _test/ — sapptest QA harness

Agent entry point. Read this **first** when you need to audit, refactor, or
ship-gate this project. Then open [PLAYBOOK.md](PLAYBOOK.md) and follow the
procedure that matches the request — verbatim, no improvising.

This folder is delivered by sappcode and pinned to a sapptest version (see
[VERSION](VERSION)). The integration contract is sapptest's
[`INTEGRATION.md`](https://github.com/localteampartners/sapptest/blob/main/INTEGRATION.md).

---

## Pick a procedure

| User asks for… | PLAYBOOK procedure | Prompt |
|---|---|---|
| Full QA sweep / `/audit` | `full-audit` | `prompts/audit.md` |
| "Why is X broken?" / find a specific bug | `bug-hunt` | `prompts/bug-hunt.md` |
| "Did this change break anything?" | `regression-check` | `prompts/audit.md` (narrowed) |
| Fix audit findings / `/refactor` | `refactor` | `prompts/refactor.md` |
| Deep check before `/ship` | `pre-ship` | `prompts/pre-ship.md` |
| Docs vs code drift / `/sync` delegate | `docs-drift` | `prompts/docs-drift.md` |

Specialist lenses used inside `full-audit`: `prompts/security.md`,
`prompts/a11y.md`, `prompts/perf.md`.

## Folder map

| Path | What it is |
|---|---|
| `PLAYBOOK.md` | Numbered procedures + shared conventions (IDs, verdict, sync). |
| `prompts/` | Self-contained prompt per task. Use as-is. |
| `templates/` | Copy these when writing findings / the verdict. |
| `schemas/` | JSON Schemas. All findings + verdicts must validate. |
| `config/` | `severity.yml`, `ship-rules.yml`, `coverage-goals.yml`, `adapters.yml`. |
| `runners/` | Shell adapters for native test runners. Normalized exit codes. |
| `findings/` | One open issue per file: `F-NNNN-<slug>.md`. Live state. |
| `reports/` | Dated audit summaries. Historical, append-only. |
| `verdict/` | `latest.md` + `latest.json` — what `/ship` reads. |
| `sync/` | TODO-sync contract (procedure lives in PLAYBOOK.md). |
| `refactor/` | PR-per-finding flow notes (procedure lives in PLAYBOOK.md). |

## Hard rules

- A full audit must **never edit application code**. Writes go only to
  `findings/`, `reports/`, `verdict/`, and tagged lines in
  `_project/TODO.md` + `_project/CURRENT_STATE.md`.
- The verdict is the source of truth for `/ship`. Apply
  `config/ship-rules.yml` — never read `findings/` directly to decide.
- Refactors open one PR per finding by default. Direct commits to the
  working branch are not allowed.
- Schema validation fails → fix the output, never relax the schema.
- Files under `findings/`, `reports/`, `verdict/` are **preserved** across
  sappcode updates. Everything else under `_test/` is replaced upstream.

## Slash commands (sappcode wrappers)

| Command | Wraps |
|---|---|
| `/audit` | `PLAYBOOK.md` → `full-audit` |
| `/verdict` | reads `verdict/latest.md` |
| `/refactor` | `PLAYBOOK.md` → `refactor` |
| `/ship` | gated on `verdict/latest.json` |
| `/sync` | delegates to `prompts/docs-drift.md` |

If a wrapper doesn't exist in this project, run the playbook directly.
