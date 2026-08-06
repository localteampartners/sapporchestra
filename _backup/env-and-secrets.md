# Playbook — env and secrets

## What "backed up" means here

Every secret the project needs to function is **(a)** documented in
`.env.example` and `_project/ENVIRONMENT.md` so you know what's
required, and **(b)** stored in **at least 2 places**, with **at least
1 off-host**. Acceptable storage locations:

- Password manager (1Password, Bitwarden, KeePassXC, …)
- Secret store (Vault, Doppler, AWS Secrets Manager, …)
- Encrypted file in an off-host backup (sops, age, ansible-vault)
- The deployed VPS itself (counts as one copy if you can SSH and read it)

A `.env` file on your laptop is **one** copy. If your laptop dies, are
the secrets recoverable? That's the test.

## Verification steps

1. **Inventory expected env vars.**

   ```bash
   test -f .env.example && cat .env.example | grep -v '^#' | grep '=' | cut -d= -f1
   ```

   - File missing or empty → soft gap. Without `.env.example`, no one
     knows what secrets the project needs.
   - List the var names; this is the "expected" set.

2. **Cross-check `_project/ENVIRONMENT.md`.**

   - Confirm the file exists and references each var from step 1.
   - Each var should have a documented source: where the canonical
     value lives (e.g. "1Password vault: sapporchestra → DB_PASSWORD").
   - Vars in `.env.example` but missing from ENVIRONMENT.md → soft gap.

3. **Confirm `.env*` is gitignored.**

   ```bash
   git check-ignore .env .env.local .env.production 2>/dev/null
   ```

   - Each file echoed back means it'd be excluded if it existed → ok.
   - No output for any of them → soft gap unless the user can confirm
     no env file with that name will ever land in this repo.

4. **No secrets in git history (heuristic).**

   ```bash
   git log -p --all 2>/dev/null | grep -iE '(api[_-]?key|secret[_-]?key|password|token|access[_-]?key)' | grep -iE '=[a-zA-Z0-9_/+]{16,}' | head -5
   ```

   - Empty output → ok.
   - Matches → **stop and read each one with the user.** Many will be
     false positives (regex examples in source, test fixtures with
     sentinel values, env-var names without values). Real leaks must
     be rotated AND the history rewritten. Don't auto-fix.

5. **Confirm off-host copy exists** (the load-bearing question).

   This is the part Claude can't verify alone. Ask the user, plainly:

   > Are the actual secret values for sapporchestra stored anywhere
   > besides this laptop? If yes, where (password manager, secret
   > store, the VPS itself)?

   - Confirms an off-host store → ok.
   - "Just on my laptop" or "in `.env`" → hard gap.
   - "Some are, some aren't" → ask which; record specific gaps per var.

6. **Last rotation date** (optional but worth recording).

   If `_project/ENVIRONMENT.md` notes when secrets were last rotated,
   note it. If it's been over a year, soft gap: rotation hygiene.

## What counts as a gap

| Severity | Condition | Example finding line |
|---|---|---|
| Hard | Secret values stored only on the developer's laptop | `[backup] env-and-secrets: no off-host copy of secret values — add to password manager` |
| Hard | Real secret found in git history | `[backup] env-and-secrets: secret leaked in commit <sha> — rotate and rewrite history` |
| Soft | `.env.example` missing or empty | `[backup] env-and-secrets: no .env.example — document required vars` |
| Soft | Vars in .env.example but not in ENVIRONMENT.md | `[backup] env-and-secrets: <var> not documented in ENVIRONMENT.md` |
| Soft | `.env*` not gitignored | `[backup] env-and-secrets: .env not in .gitignore` |
| Soft | Last rotation > 1y or unknown | `[backup] env-and-secrets: secrets not rotated since <date>` |

## What to put in BACKUP.md

- Hard gaps unresolved → **gaps**
- All hard closed, soft may remain → **ok** (note soft gaps in TODO)
- Couldn't ask the user (running unattended via `/loop`) → **unverified**
  with a note: "needs a human pass for off-host confirmation."
