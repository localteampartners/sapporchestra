# ENVIRONMENT — sapporchestra

<!-- UPDATE WHEN: an env var is added, renamed, removed, or its source/owner changes. Also update .env.example and .env.template in the same edit. -->

Every env variable the project reads, what it's for, and **where the real
value lives**. If the project uses sappvault, "where it lives" is
`sappvault://sapporchestra/NAME` (macOS Keychain). Otherwise it's a
password manager or provider dashboard.

The real secrets do **not** live in this file — only pointers to them.

---

## Required

| Name | Purpose | Where the real value lives | Used by |
|---|---|---|---|
| `<!-- FILL IN -->` | <!-- FILL IN --> | <!-- e.g., "1Password > sapporchestra > DB_URL" --> | <!-- e.g., "api, worker" --> |
|  |  |  |  |

## Optional

| Name | Purpose | Default | Where the real value lives |
|---|---|---|---|
| `<!-- FILL IN -->` | <!-- FILL IN --> | <!-- FILL IN --> | <!-- FILL IN --> |
|  |  |  |  |

## Where env vars are loaded

- **Local:** <!-- FILL IN: e.g., ".env file at project root, loaded via dotenv" -->
- **Production:** <!-- FILL IN: e.g., "systemd EnvironmentFile=/etc/sapporchestra.env", "Fly.io secrets", "Vercel dashboard" -->

## Rotation notes

- Which vars rotate on a schedule: <!-- FILL IN or "none" -->
- Which vars would break production if rotated without redeploy: <!-- FILL IN -->

## Keep `.env.example` and `.env.template` in sync

- `.env.example` — documentation; lists every variable with placeholder/example values.
- `.env.template` — sappvault input; lists every variable with `${vault:NAME}` for secrets and plain values for non-secret config.

When you add a var here, add it to both files in the same edit. To materialize the live `.env` after editing the template, run `sappvault inject .env.template .env` (or `/vault` in Claude Code).
