---
description: Security scan — find hardcoded secrets, dangerous code patterns, vulnerable dependencies, and insecure config; emits the ship-gating security verdict. Offline, seconds.
---

Run **sappsecurity** against this project and report the verdict.

`sappsecurity` answers the question the other gates don't: *is anything in
this codebase unsafe to ship?* It scans for hardcoded secrets (values always
redacted), injection-prone patterns, weak crypto, vulnerable dependencies
(`npm audit`), and insecure config, ranks findings by severity, and exits
non-zero when the verdict fails. `/ship` runs the same verdict as a gate.

## Steps

1. **Preflight.** Confirm `sappsecurity` is on PATH (`sappsecurity --version`).
   If it isn't, tell the user to install it once:
   `cd ~/tools/sappsecurity && npm install && npm run build && npm link`.

2. **Run.** Execute `sappsecurity scan . $ARGUMENTS` from the project root.
   - `--format md --output _project/SECURITY_SCAN.md` to keep a copy.
   - `--only secrets,patterns` to scope; `--deep` for the optional LLM review
     (needs `ANTHROPIC_API_KEY`).
   - WordPress projects: add `--no-vendored`.

3. **Triage findings with the user.**
   - **critical / high** — fix now; these fail the verdict. Secrets get
     rotated *and* moved to sappvault (`/vault`), never just deleted from code.
   - **medium / low** — judgment calls; rules are recall-biased, so false
     positives happen. Genuine non-issues get suppressed in
     `sappsecurity.config.json` (ignore paths / rule overrides) with a comment
     saying why — never by deleting the finding from a report.

4. **Re-run the verdict.** `sappsecurity verdict .` — exit 0 means the gate
   passes (default threshold: any critical or high fails; tune with
   `--fail-on`). Report the result.

5. **Never weaken the gate to get past it.** Threshold changes or rule
   suppressions are user decisions, made in config, visible in the diff.
