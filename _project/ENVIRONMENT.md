# ENVIRONMENT — sapporchestra

<!-- UPDATE WHEN: an env var is added, renamed, removed, or its source/owner changes. Also update .env.example and .env.template in the same edit. -->

Every env variable the project reads, what it's for, and **where the real
value lives**. If the project uses sappvault, "where it lives" is
`sappvault://sapporchestra/NAME` (macOS Keychain). Otherwise it's a
password manager or provider dashboard.

The real secrets do **not** live in this file — only pointers to them.

---

## Required

None.

## Optional

No secrets: sapporchestra is a desktop/embedded plugin. Every variable below
is plain configuration, read directly by the plugin, the CLI and the headless
harness, and each has a working default.

| Name | Purpose | Default | Read by |
|---|---|---|---|
| `SAPP_SFZ_ROOT` | Samples root the `instrument` choice list is enumerated from. Wins over the shared Sapp `samplesRoot` setting, so a station box needs no user settings file. | the shared Sapp setting, else `~/Samples` | plugin, CLI, headless harness |
| `SAPP_SFZ_RESCAN` | `1` = rescan the samples root and rewrite `<root>/.sapp-sfz-index.json` at plugin construction. The unattended equivalent of the editor's rescan (issue #1). Costs a full tree walk, so it is opt-in per run. | unset (use the cached index) | plugin, headless harness |
| `SAPP_ORCHESTRA_LOG` | File to append the plugin's diagnostic lines to (`SappOrchestra-build`, `-instrument`, `-audio-source`, `-sfz-index`). They always also go to the host's JUCE logger, and to stderr on Windows. | unset | plugin, headless harness |

## Where env vars are loaded

- **Local:** exported in the shell, or set by the driving script.
- **Station boxes:** set by the sappradio host launcher before it loads the
  plugin (see that repo's WINDOWS_SETUP handbook).

## Rotation notes

- None rotate; none are secrets, so there is nothing to redeploy after a change.

## Keep `.env.example` and `.env.template` in sync

- `.env.example` — documentation; lists every variable with placeholder/example values.
- `.env.template` — sappvault input; lists every variable with `${vault:NAME}` for secrets and plain values for non-secret config.

When you add a var here, add it to both files in the same edit. To materialize the live `.env` after editing the template, run `sappvault inject .env.template .env` (or `/vault` in Claude Code).
