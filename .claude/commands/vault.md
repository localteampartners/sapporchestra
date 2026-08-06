---
description: Manage this project's secrets via sappvault (status / add / inject / rotate).
---

Drive the sappvault secrets workflow for this project. sappvault stores
secrets in macOS Keychain — you never see the values, you orchestrate.

## Hard rules (do not break these)

1. **You do not type, paste, or read secret values.** Every value-taking
   sappvault command (`set`, `add`, `import`, `setup`, `rotate`) will
   refuse to run under you — that's by design.
2. **Never run `sappvault get NAME --reveal`** unless the user has just
   asked you to display the value.
3. **Never write `.env` by hand.** Use `sappvault inject`.
4. **Never ask the user to paste a value into the chat.** Tell them to
   run the command themselves.

## Step 1 — verify the toolkit is installed

```bash
command -v sappvault >/dev/null || {
  echo "sappvault not on PATH"
  echo "Install it: cd ~/tools/sappvault && ./install.sh"
  exit 1
}
sappvault doctor
```

If `sappvault doctor` fails, stop and ask the user to fix the
installation. Don't try to work around it.

## Step 2 — figure out what the user wants

Pick the sub-flow based on the user's request. If unclear, ask.

| User says… | Sub-flow |
|---|---|
| "what secrets does this project have?" / "status" | **Status** |
| "I need to add a key / password" / "set up env" | **Add** |
| "I already have a .env" / "import these" | **Import** |
| "generate the .env" / "the app won't start" | **Inject** |
| "rotate the X key" / "I rolled X" | **Rotate** |

## Sub-flow: Status

```bash
[ -f .sappvault ] && cat .sappvault || echo "(no .sappvault marker)"
sappvault list
[ -f .env.template ] && grep -oE '\$\{vault:[A-Za-z0-9_.-]+\}' .env.template | sort -u || echo "(no .env.template)"
```

Report: project name, secrets currently in vault, placeholders in
`.env.template`. Flag any placeholder not in `list` as missing.

## Sub-flow: Add (new secrets needed)

1. If no `.sappvault` exists: `sappvault init <project-name>`.
2. If no `.env.template` exists: draft one with the user. Use
   `${vault:NAME}` for each secret. Commit it. Add `.env` to
   `.gitignore`.
3. For each missing placeholder, tell the user (verbatim — don't
   paraphrase):

   > "Please run this in your terminal — it walks the template and
   > prompts for every missing value with hidden input:
   >
   > ```
   > sappvault setup
   > ```
   >
   > Tell me when it's done and I'll generate `.env`."

4. After the user confirms, verify with `sappvault list`, then run
   `sappvault inject .env.template .env` and check the result with
   `ls -la .env` (expect `0600`).

If the user offers to share a value in the chat, decline and redirect
them to `sappvault set NAME` in their terminal.

## Sub-flow: Import (existing .env)

Tell the user:

> "Run `sappvault import <path-to-your-existing-.env>` in your terminal.
> It will prompt to confirm each key. When it's done, delete the source
> file with `rm <path>` and I'll regenerate `.env` from the template."

Then `sappvault inject .env.template .env`.

## Sub-flow: Inject (re-materialize .env)

```bash
sappvault inject .env.template .env
ls -la .env
```

If `inject` exits non-zero with "missing secrets", run the **Add**
sub-flow for the missing names.

## Sub-flow: Rotate (replace a value)

Tell the user:

> "Please run `sappvault rotate <NAME>` in your terminal. After it
> finishes, I'll regenerate `.env` and remind you to restart the
> service."

Then `sappvault inject .env.template .env` and remind the user to
restart whatever process reads `.env`.

## Step 3 — log what changed

After any change:
- Update `_project/CURRENT_STATE.md` if a new secret category was added.
- Add a dated entry to `_project/CHANGELOG.md` ("added DATABASE_URL to
  vault, regenerated .env").
- Do not record the *values*. Names only.

## The GUI

If the user asks about a GUI: macOS Keychain Access.app is the GUI.
Open it with `sappvault gui` and search `sappvault:` to filter. A
custom GUI is not planned.
