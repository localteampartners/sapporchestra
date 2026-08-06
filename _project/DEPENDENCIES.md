# DEPENDENCIES — sapporchestra

<!-- UPDATE WHEN: an external service/API is added or removed, an account changes hands, billing changes, or credentials rotate -->

External systems this project depends on. If access to any of these is lost,
part or all of the project stops working. This file tells you what to recover.

Runtime library dependencies live in `package.json` / `requirements.txt` /
similar — don't duplicate them here.

---

## External services

| Service | What it does | Account (email) | Paid with | Monthly cost | Status page |
|---|---|---|---|---|---|
| <!-- e.g., Stripe --> | <!-- e.g., payments --> | <!-- email on the account --> | <!-- card ending 1234 --> | <!-- $X/mo --> | <!-- URL --> |
|  |  |  |  |  |  |

## Domain / DNS

See [INFRASTRUCTURE.md](INFRASTRUCTURE.md) for the primary domain. If this
project uses additional domains or subdomains from different registrars, list
them here.

- <!-- FILL IN or "none" -->

## APIs & credentials

For each external API, point to where the credentials live (never paste them here).

| API | Credential type | Where it lives |
|---|---|---|
| <!-- e.g., OpenAI --> | <!-- API key --> | <!-- e.g., "1Password > sapporchestra > OPENAI_API_KEY" --> |
|  |  |  |

## Single points of failure

If any one of these goes down or we lose access, what breaks?

- <!-- FILL IN: e.g., "Stripe down → checkout broken (read-only mode still works)" -->
- 

## Account recovery

Who / what can recover access if the primary account is locked out?

- <!-- FILL IN: e.g., "all provider accounts use localteampartners@gmail.com; recovery email is X" -->
- 
