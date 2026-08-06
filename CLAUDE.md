# CLAUDE.md — sapporchestra

Read this file on session start. Read other files **only when the task requires them** — every file you open is tokens spent.

---

## When to read what

| Task | Files to open |
|---|---|
| Resume after a dead session, compaction, or time away | `_project/HANDOFF.md` **first** (or run `/resume`) |
| How we work here (deploy style, version pins, gotchas) | `_project/CONVENTIONS.md` |
| New feature / behavior change | `_project/SPEC.md`, `_project/ARCHITECTURE.md` |
| Bug fix / "why is X broken" | `_project/CURRENT_STATE.md`, then relevant code |
| Deploy, run, operate the service | `_project/RUNBOOK.md`, `_project/INFRASTRUCTURE.md` |
| Add env var or external service | `_project/ENVIRONMENT.md`, `_project/DEPENDENCIES.md`, `.env.template` |
| Pick a library or pattern | `_project/DECISIONS.md` |
| Plan next work (tactical) | `_project/TODO.md`, `_project/CURRENT_STATE.md` |
| Think about direction (themed, multi-month) | `_project/ROADMAP.md` |
| Cold open, no specific task | `_project/HANDOFF.md`, `_project/README.md`, `_project/CURRENT_STATE.md` |

Don't pre-read everything. Pull context as needed.

---

## Keep the docs current (routing table)

| When you… | Update |
|---|---|
| Start a multi-step task | `_project/HANDOFF.md` — write the plan **before** the first edit |
| Complete a step, hit a gotcha, or rule out an approach | `_project/HANDOFF.md` — tick / note it as you go, not at the end |
| Learn a non-obvious workflow fact (deploy quirk, version pin, "never do X") | `_project/CONVENTIONS.md` |
| Ship a feature | `_project/CURRENT_STATE.md`, `_project/CHANGELOG.md`, `_project/TODO.md`; reset `_project/HANDOFF.md` to "none" |
| Make a non-obvious design choice | `_project/DECISIONS.md` |
| Add / rename / remove an env var | `_project/ENVIRONMENT.md`, `.env.example`, `.env.template` |
| Add an external service / API / account | `_project/DEPENDENCIES.md` |
| Change how to run / deploy / rollback | `_project/RUNBOOK.md` |
| Change stack / components / data flow | `_project/ARCHITECTURE.md` |
| Change scope / goals | `_project/SPEC.md` |
| Shift project direction, sequence, or theme | `_project/ROADMAP.md` |
| Discover something broken or half-done | `_project/CURRENT_STATE.md` (known issues) |
| Add or remove the chatbot integration | `.sappchatbot/config.json`, `.claude/settings.json` (hooks), `.claude/commands/inbox.md` |
| Harden, audit, or re-audit the VPS | run `/harden-vps`; review the `_test/findings/`-style audit output; record the date in `_project/CHANGELOG.md` |
| Run a sapptest audit | `_test/findings/`, `_test/reports/`, `_test/verdict/`, plus tagged lines in `_project/TODO.md` + `_project/CURRENT_STATE.md` (via sync) |
| Run a sappverify check (`/verify`) | `_verify/verdict/`, `_verify/reports/`; if a function is added/changed, `_verify/functions.yml` (the contract) |
| Run a security scan (`/scan`) or suppress a finding | `sappsecurity.config.json` (suppressions need a why); note fixed findings in `_project/CHANGELOG.md` |
| Open a refactor PR for an audit finding | linked finding stays open until the next `/audit` verifies it; PR description references `F-NNNN` |
| Bump sapptest version | `_test/VERSION`; note in `_project/CHANGELOG.md`; re-run `/audit` |
| Change ship rules | `_test/config/ship-rules.yml`; note in `_project/DECISIONS.md` |

Rules: edit, don't append. Update in the same session as the change. Date CHANGELOG / DECISIONS entries `YYYY-MM-DD`. Each `_project/*` file has its own `<!-- UPDATE WHEN: -->` trigger at the top.

**Automatic reminder:** a Stop hook (`.claude/hooks/doc-nudge.sh`) prints a one-line nudge when the working tree has 3+ source-file changes but no `_project/*.md` updates this session. If you see it, consult the table above and update the right file before moving on.

---

## Session continuity

Durable working state lives in files, not in this chat. Assume the session can die or compact at any moment:

- Before the first edit of any multi-step task, write the plan into `_project/HANDOFF.md`.
- Update it as you go — tick completed steps, record gotchas and ruled-out approaches the moment they happen.
- A brand-new session must be able to resume from `_project/HANDOFF.md` alone. If it can't, the handoff is behind — fix it now.
- `/handoff` checkpoints on demand; `/resume` re-enters. When work ships, reset the handoff to `Work in flight: none`.

---

## Commands

- `/ship` — finished a feature: checks the sapptest + sappverify verdicts, runs the live security verdict (`sappsecurity verdict .`), runs verify, updates docs, commits. Refuses on `BLOCK` or a failing security verdict (overrides: `--no-audit`, `--no-verify`, `--no-security` — always explicit, never silent).
- `/sync` — adaptive drift check (samples git first, only re-reads affected docs). Delegates to `_test/prompts/docs-drift.md` if sapptest provides one.
- `/resume` — pick up where the last session left off: reads `_project/HANDOFF.md` first, briefs in ≤10 lines, continues the plan (falls back to a CURRENT_STATE + TODO cold-open brief when nothing is in flight).
- `/handoff` — checkpoint the current session into `_project/HANDOFF.md` so any future session can resume without this conversation. Run before ending mid-task or before anything risky.
- `/update-sappcode` — pull the latest sappcode template and merge new files / updated tooling into this project. Safe: never overwrites user-content files; prompts before touching tooling files you may have customized.
- `/audit` — sapptest full audit. Writes findings, verdict, and TODO sync. Never edits app code.
- `/verdict` — print the latest sapptest verdict from `_test/verdict/latest.md`.
- `/refactor` — open one PR per refactor-marked sapptest finding. Default mode is per-finding; batched is opt-in via `_test/config/ship-rules.yml`.
- `/harden-vps` — drive the sappsecurevps playbook (audit + harden) against this project's VPS. Reads `.vps-proxy.json` for the profile; toolkit lives at `$SAPPSECUREVPS_DIR` (default `~/tools/sappsecurevps`). Audit is read-only; hardening prompts at each destructive gate and auto-rolls back on SSH failure.
- `/scan` — sappsecurity static scan (secrets, dangerous patterns, vulnerable deps, insecure config). Offline, seconds. `sappsecurity verdict .` is the ship gate; suppressions live in `sappsecurity.config.json` and are user decisions.

`./verify.sh` is the fast-feedback script — typecheck + lint + tests in under a minute. Run it after any non-trivial change.

---


## Git commits

Do not add `Co-Authored-By: Claude` (or any AI co-author / "Generated with Claude Code" trailer) to commit messages, PR descriptions, or anything similar. Author + message only.

---

## Style

Short, present tense, scannable. Placeholders look like `<!-- FILL IN: -->` or `<TBD>` — replace them, don't leave them. Files describe the project's current state, not session history.
