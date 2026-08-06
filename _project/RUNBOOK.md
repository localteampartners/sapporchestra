# RUNBOOK — sapporchestra

<!-- UPDATE WHEN: any command here stops working, or a new operational task becomes routine enough to document -->

The authoritative source for "how do I operate this thing?"

---

## Run locally

### One-time setup

```bash
# <!-- FILL IN: clone, install deps, create .env from .env.example, run migrations -->
```

### Start the app

```bash
# <!-- FILL IN: e.g., npm run dev -->
```

### Run tests

```bash
# <!-- FILL IN -->
```

---

## Deploy

**Hosting:** see [INFRASTRUCTURE.md](INFRASTRUCTURE.md) for the *where*.
This section is the *how*.

```bash
# <!-- FILL IN: deploy command or step-by-step -->
```

### Rollback

```bash
# <!-- FILL IN: how to revert a bad deploy -->
```

---

## Operate (if there's a VPS / running service)

### Check it's alive

```bash
# <!-- FILL IN: healthcheck URL, or ssh + systemctl status -->
```

### Restart

```bash
# <!-- FILL IN -->
```

### Tail logs

```bash
# <!-- FILL IN -->
```

---


## Debug checklist

When something's broken, try these in order:

1. <!-- FILL IN: e.g., "check the healthcheck endpoint" -->
2. <!-- FILL IN: e.g., "tail the last 200 log lines" -->
3. <!-- FILL IN: e.g., "verify env vars match ENVIRONMENT.md" -->
4. <!-- FILL IN: e.g., "check external service status pages (see DEPENDENCIES.md)" -->
