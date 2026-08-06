<!-- UPDATE WHEN: the data layer changes — wired/unwired, provider change,
     schedule installed, first successful drill. Edit the table; don't rewrite. -->

# Data layer — sapporchestra

How this project's database is backed up, drilled, and monitored, via
**sappdata** (the suite's data layer). sappdata produces DB-complete backup
bundles, hands them to sappbackup for storage, restores them to an ephemeral
DB to prove they work, and posts DB-health signals to the monitor.

Run `/data` in Claude Code to drive it. Full tool docs: `~/tools/sappdata`.

## Status

| Aspect | State | Last checked |
|---|---|---|
| Provider | `<unwired>` — run `/data wire` | — |
| `data:` block in sapp.yml | no | — |
| DB creds in sappvault | no | — |
| Scheduled backup + drill | no | — |
| Last successful drill | never | — |

## Provider

`<unwired>` — `/data wire` auto-detects Supabase / Postgres / SQLite and
writes the `data:` block. If this project has no database, it stays unwired
and that's correct (nothing to back up).

## Credentials

Supabase/Postgres backups need creds in this project's vault. Never plaintext,
never pasted into chat:

```
pbpaste | sappvault set sapporchestra DATABASE_URL --from-file -
pbpaste | sappvault set sapporchestra SUPABASE_SERVICE_ROLE_KEY --from-file -   # Supabase only
```

The non-secret `supabase_url` / `project_ref` / sqlite `file` live in
`sapp.yml` and are fine to commit. Only the connection string and service-role
key go in the vault.

## Notes

- Backups stream DB → file → sappbackup; data never passes through the agent.
- For Supabase, the bundle captures schema + data + Storage buckets + Auth
  users + RLS policies — beyond a plain `pg_dump`.
- An untested backup is a hope: `/data drill` restores to a throwaway DB and
  verifies before you trust it.
