---
description: Ship a finished feature — run verify, update docs, commit.
---

End-of-feature workflow. One commit covers code + doc updates together.

## Steps

1. **Check the sapptest verdict.** If `_test/verdict/latest.json` exists,
   read it. Apply this gate:
   - `verdict: "SHIP"` → proceed.
   - `verdict: "SHIP WITH CAVEATS"` → require the user to explicitly
     acknowledge (`--ack-caveats` in the slash invocation, or a clear
     "yes, ship with caveats" answer to your prompt). If they don't,
     stop.
   - `verdict: "BLOCK"` → refuse. Tell the user to run `/audit` and
     fix the blocking findings, or to override explicitly with
     `--no-audit` (only in genuinely pre-audit projects).
   - **Staleness:** compare `audit_date` to the latest commit on this
     branch. If the latest commit is newer, treat the verdict as stale
     and require a fresh `/audit` (or explicit `--no-audit` override).

   If `_test/verdict/latest.json` is missing entirely, sapptest isn't
   installed — proceed with the rest of `/ship` as before, but mention
   it once.

1b. **Check the sappverify verdict.** If `_verify/verdict/latest.json`
   exists, read it (functional acceptance — *does the running app do
   everything it should?*). Apply the same gate on its `verdict` field:
   - `verdict: "PASS"` → proceed.
   - `verdict: "SHIP WITH CAVEATS"` → only non-critical functions failed;
     require explicit acknowledgement (`--ack-caveats`, or a clear
     "yes, ship with caveats"). If not given, stop.
   - `verdict: "BLOCK"` → refuse. A critical function failed, a declared
     function is unchecked, a check errored, or the run is stale. Each
     `blocking[]` entry has an `id`, a `reason`, and a `screenshot`. Tell
     the user to run `/verify` and fix the blocking function(s), or to
     override explicitly with `--no-verify` (only in genuinely pre-verify
     projects).
   - **Staleness:** compare the verdict's `commit` / `generated` to the
     latest commit on this branch. If the branch is newer, treat the
     verdict as stale and require a fresh `/verify` (or `--no-verify`).

   If `_verify/verdict/latest.json` is missing, sappverify isn't installed
   — proceed, but mention it once. When both verdicts exist, ship only if
   **neither** is `BLOCK` and any caveats on either are acknowledged.

1c. **Run the security verdict.** If `sappsecurity` is on PATH, run
   `sappsecurity verdict .` from the project root (offline, seconds — it
   runs live, so there is no staleness to check):
   - exit `0` → proceed.
   - exit `1` → refuse. Blocking security findings exist (critical/high by
     default). Tell the user to run `/scan`, fix or explicitly suppress
     findings in `sappsecurity.config.json`, or override with
     `--no-security` (note the override in the commit body:
     "shipped with --no-security").
   - exit `2` → the scan itself failed to run; report the error, treat as
     not-installed (proceed, mention it once).

   If `sappsecurity` is not on PATH, proceed — but mention once that the
   security gate is not installed
   (`cd ~/tools/sappsecurity && npm install && npm run build && npm link`).
   Never edit findings, thresholds, or config yourself to get past the
   gate — that's the user's call, visible in the diff.

2. **Verify.** If `./verify.sh` exists, run it. If it fails, stop and report — don't ship broken code. If it doesn't exist, note that and continue.

2. **Ask the user for a one-line summary** of what shipped. Use it as the commit subject.

3. **Update docs** per the routing table in `CLAUDE.md`. At minimum:
   - `_project/CURRENT_STATE.md` — move the feature from "in progress" → "built and working"; bump the "Last verified" date to today.
   - `_project/CHANGELOG.md` — add an entry under **Unreleased** (or under today's date if this is also a deploy).
   - `_project/TODO.md` — remove the task from "Next up"; add it to "Done (recent)".

   If the feature also touched env vars, external services, decisions, architecture, or infra — update those files too. Consult the routing table.

4. **Show the doc diff.** Before staging, show the user what you're about to change across `_project/*` and ask for approval.

5. **Commit.** After approval, `git add -A` and make one commit. Subject = the one-line summary from step 2. Add a short body if useful.

6. **Don't push.** Only push if the user explicitly asks.

## Rules

- One commit per ship — code and doc updates go together.
- **Never ship if the verdict is `BLOCK`** without explicit `--no-audit`.
- **Never ship on a failing security verdict** without explicit
  `--no-security`.
- **Never ship if `verify.sh` failed.**
- **Never silently rewrite the verdict** to bypass `BLOCK`. If the user
  insists on shipping, they override explicitly — the verdict stays
  honest.
- If only docs changed and no code, say so explicitly before committing.
- Don't invent updates — if a file isn't affected, leave it alone.
