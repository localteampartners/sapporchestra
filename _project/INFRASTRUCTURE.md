# INFRASTRUCTURE — sapporchestra

<!-- UPDATE WHEN: hosting changes, the VPS is provisioned/migrated/destroyed, DNS changes, or SSH access details change -->

Answers "where does this live and how do I reach it?" at a glance.

---

## Is there a VPS / hosted deployment?

<!-- FILL IN: YES or NO -->

If **NO**, this project only runs locally. You can leave the rest of this file blank
or delete it — but keeping the "NO" answer above is useful so future-you doesn't
have to go looking.

---

## VPS / host details

(Only fill in if answer above is YES.)

- **Provider:** <!-- FILL IN: e.g., Hetzner, DigitalOcean, Fly.io, Vercel, Railway -->
- **Host name / label:** <!-- FILL IN: the name you gave it in the provider dashboard -->
- **IP address:** <!-- FILL IN or "dynamic" -->
- **Region:** <!-- FILL IN -->
- **Size / tier:** <!-- FILL IN: e.g., "2 vCPU / 4GB RAM", "Hobby plan" -->
- **Monthly cost:** <!-- FILL IN -->
- **Paid with:** <!-- FILL IN: which card / account — so you know what expires -->
- **Provisioned on:** <!-- FILL IN: YYYY-MM-DD -->

## How to SSH / access

```bash
# <!-- FILL IN: ssh command, or URL to provider dashboard if serverless -->
# e.g., ssh -i ~/.ssh/hetzner_sapporchestra root@1.2.3.4
```

SSH key location: <!-- FILL IN: e.g., ~/.ssh/id_ed25519 -->

## Domain & DNS

- **Primary domain:** <!-- FILL IN or "none" -->
- **Registrar:** <!-- FILL IN: e.g., Namecheap, Cloudflare -->
- **DNS provider:** <!-- FILL IN -->
- **TLS cert:** <!-- FILL IN: e.g., "Let's Encrypt via Caddy", "Cloudflare" -->

## What runs on the box

Services / processes that live on this host.

- <!-- FILL IN: e.g., "systemd unit `sapporchestra.service` runs the Node app on :3000" -->
- <!-- FILL IN: e.g., "Caddy reverse-proxies :443 → :3000" -->
- <!-- FILL IN: e.g., "Postgres 16 on :5432 (local only)" -->

## Backups

- **What's backed up:** <!-- FILL IN: e.g., "Postgres dump nightly", "nothing (stateless)" -->
- **Where backups live:** <!-- FILL IN: e.g., "S3 bucket `xyz-backups`" -->
- **Retention:** <!-- FILL IN -->
- **Last tested restore:** <!-- FILL IN: YYYY-MM-DD or "never" -->

## If I lose access

Recovery checklist if credentials are lost:

1. <!-- FILL IN: e.g., "log into provider dashboard with recovery email" -->
2. <!-- FILL IN: e.g., "reset root password via console" -->
3. <!-- FILL IN: e.g., "re-add SSH key from 1Password item 'proj-ssh'" -->
