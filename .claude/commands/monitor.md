---
description: Generate or refresh .monitor.yml for the sapplab monitor dashboard (https://monitor.sapplab.net).
---

Fill in `.monitor.yml` at the project root so this project can be registered
with the sapplab monitor dashboard at https://monitor.sapplab.net.

## Source of truth

The schema + placeholder values already live in this project's `.monitor.yml`
(shipped by the sappcode template). Use that file as the schema template —
edit it in place. Do **not** fetch a remote schema; the local file is authoritative.

## Rules

1. `schema: 1` — do not change.
2. `id` — REQUIRED. lowercase-kebab, stable, globally unique across the
   user's projects. Regex: `^[a-z0-9][a-z0-9-]*$`. Derive from the repo /
   directory name; ask the user if ambiguous.
3. `name`, `description`, `purpose` — REQUIRED. Read `README.md`,
   `package.json`, `pyproject.toml`, `Cargo.toml`, `go.mod`, and any
   `_project/SPEC.md` or `_project/ARCHITECTURE.md` to get these right.
   **Do not invent features the code doesn't actually have.**
4. `urls.primary` — REQUIRED and must be a real, reachable URL. If this
   project isn't deployed, set `status: paused` and omit `urls.primary` —
   then tell the user so they can fill it in when it ships.
5. `host.*` — inspect deploy configs, systemd units, PM2 ecosystem files,
   Dockerfiles, and any `_project/INFRASTRUCTURE.md` or
   `_project/RUNBOOK.md` to fill in `vps`, `path`, `process_manager`,
   `pm2_name`, `nginx_site`. Leave fields blank if genuinely unknown —
   **do not guess.**
6. `stack.*` — pull from actual dependency manifests. Only list libraries
   that meaningfully shape the code, not every transitive dep.
7. `checks` — define at least one `http` check against `urls.primary`
   (GET, expect 200, 60s interval). Add more checks if the project has a
   dedicated `/healthz`, `/status`, a background worker reachable via TCP,
   or separate subdomains.
8. **NEVER put credentials in this file.** `headers` must not contain
   bearer tokens, API keys, or cookies. If a health check needs auth,
   leave a note in the `notes` field so the user configures it inside the
   monitor app itself.
9. Keep the file under ~120 lines. Remove every commented-out example
   block — ship a lean, real file.
10. **YAML gotcha:** any `features` bullet, `notes` line, or string that
    contains a colon (`:`) MUST be wrapped in double quotes — otherwise
    YAML parses it as a key/value map and upload fails. Example:
    `- "Post editor: text, images, video"`.

## Process

1. **Scan the repo.** README, `package.json` / `pyproject.toml` / `Cargo.toml` /
   `go.mod`, `_project/*.md`, deploy / infra configs (Dockerfile, systemd
   unit files, PM2 ecosystem files, nginx configs).
2. **Draft the file content** based on the scan.
3. **Validate mentally** against the schema in the existing `.monitor.yml`
   (the placeholder version shipped with the template). Especially check:
   - `id` matches `^[a-z0-9][a-z0-9-]*$`
   - `urls.primary` is a full URL with scheme
   - No credentials in `headers`
4. **Overwrite `./.monitor.yml`** with the completed version.
5. **Print a one-paragraph summary** of:
   - what you inferred and from where,
   - any fields you left blank and why,
   - the exact URL and interval of each check you defined,
   - next step: *"paste the contents into https://monitor.sapplab.net → Add project."*

**Do NOT register it with the dashboard yourself.** Just produce the file.

## When to re-run `/monitor`

- After any change that affects health-check targets (new URL, new `/healthz`, new subdomain)
- After migrating process manager / host / port
- After major stack changes (new framework, new database)
- After the project's description or feature set materially changes

Re-running overwrites `.monitor.yml` in place; re-submit the updated file to
the dashboard — the monitor upserts by `id`.
