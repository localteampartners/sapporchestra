# ARCHITECTURE — sapporchestra

<!-- UPDATE WHEN: tech stack changes, a component is added/removed, data flow changes, or a major directory is renamed -->

## Tech stack

- **Language / runtime:** <!-- FILL IN: e.g., Node 20, Python 3.12, Go 1.22 -->
- **Framework:** <!-- FILL IN: e.g., Next.js 15, FastAPI, Express -->
- **Database:** <!-- FILL IN: e.g., Postgres 16, SQLite, none -->
- **Key libraries:** <!-- FILL IN: only ones that meaningfully shape the code -->
- **Frontend:** <!-- FILL IN or "n/a" -->
- **Build / package manager:** <!-- FILL IN: npm, pnpm, uv, cargo -->

## Components

High-level blocks and what each is responsible for.

- **<!-- FILL IN: component name -->** — <!-- FILL IN: what it does -->
- 
- 

## Data flow

How a request / event moves through the system.

```
<!-- FILL IN: ascii diagram, or a numbered list like: -->
<!-- 1. User hits /api/foo -->
<!-- 2. Handler validates with Zod, then calls FooService -->
<!-- 3. FooService reads from Postgres via Drizzle -->
<!-- 4. Response serialized + returned -->
```

## Key directories

Only list directories whose purpose isn't obvious from the name.

| Path | Purpose |
|---|---|
| `<!-- FILL IN -->` | <!-- FILL IN --> |
|  |  |

## External touchpoints

What this project talks to across the network. See [DEPENDENCIES.md](DEPENDENCIES.md)
for account/billing details.

- <!-- FILL IN: e.g., "Stripe API for payments", "OpenAI for embeddings" -->
- 

## Known sharp edges

Architectural things that will bite someone if they don't know about them.

- <!-- FILL IN: e.g., "Worker must finish within 10s or Cloudflare kills it" -->
- 
