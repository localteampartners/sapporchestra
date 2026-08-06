---
description: Drive the sappsecurevps hardening playbook against this project's VPS.
---

Walk the user through the full sappsecurevps `AGENT.md` playbook against
the VPS this project points at. The audit is read-only and safe; the
hardener has lockout-recovery gates and rolls back automatically on
SSH failure, but it's destructive — get explicit user approval at each
gate.

## Prerequisites — run preflight.sh

sappsecurevps ships a single canonical pre-flight script — use it.
Don't reimplement the checks inline.

1. **Profile name:** read `.vps-proxy.json` at the project root (key
   `profile`). If missing, ask the user which vps-proxy profile to use.
   Bind it to a shell variable for the rest of the session
   (e.g. `PROFILE=<value>`).

2. **Toolkit location:** sappsecurevps lives at `$SAPPSECUREVPS_DIR`
   (default `~/tools/sappsecurevps`). The hardening scripts
   are at `$SAPPSECUREVPS_DIR/vps-hardening-toolkit/`. If the directory
   doesn't exist, abort and tell the user to clone the repo:
   `git clone <repo-url> $SAPPSECUREVPS_DIR`.

3. **Run preflight:**
   ```bash
   cd $SAPPSECUREVPS_DIR/vps-hardening-toolkit
   ./remote/preflight.sh --profile $PROFILE --users $USER
   ```

   Map the exit code to a specific user message — don't print a
   generic "fix vps-proxy":

   | Exit | Meaning | Tell the user |
   |---|---|---|
   | 0 | all checks passed | proceed to step 1 below |
   | 4 | vps-proxy not on PATH | install vps-proxy: https://github.com/localteampartners/vpsproxy |
   | 5 | profile `$PROFILE` not registered with vps-proxy | add it to `~/.vps-proxy/config.json` |
   | 6 | host unreachable | check `~/.ssh/config` host alias; try `ssh-copy-id -i ~/.ssh/id_ed25519.pub <ssh-host>` |
   | 7 | no trusted user has `authorized_keys` | run `ssh-copy-id` for the trusted user before continuing — `lockdown-ssh.sh` would refuse later anyway |
   | other | usage error or unexpected | show the script's stderr and stop |

   Watch for these markers in the script's output (parsed but also
   human-readable):
   - `@@ PREFLIGHT_START profile=<name>` — running.
   - `@@ PREFLIGHT_KEY user=<u> status=OK|MISSING` — per-user key check.
   - `@@ PREFLIGHT_FAIL reason=<r> [extras]` — abort and report.
   - `@@ PREFLIGHT_OK profile=<name> host=<sshHost> users_with_keys=<n>` — go.

## Step 1 — open the SSH ControlMaster (lockout safety net)

```bash
cd $SAPPSECUREVPS_DIR/vps-hardening-toolkit
./remote/setup-control-master.sh --profile $PROFILE
```

This modifies `~/.ssh/config` to add a ControlMaster block for the
profile's `sshHost`. If the user pushes back on touching `~/.ssh/config`,
explain that this is the one exception in `AGENT.md` and is the rollback
mechanism. Don't proceed without it.

## Step 2 — sync the toolkit to the remote

```bash
./remote/sync-toolkit.sh --profile $PROFILE
```

Watch for `@@ SYNC_DONE host=... dest=...`. If sync fails, stop.

## Step 3 — run the read-only audit

```bash
./remote/remote-audit.sh --profile $PROFILE
```

Three outcomes:

| Exit code | Meaning | Next step |
|---|---|---|
| `0`, no findings | Clean. | Proceed to step 4. |
| `0`, findings | Review with user. Some findings are benign. | Show findings to user, ask: "Proceed with hardening?" |
| `2` | **Compromise indicators.** | **STOP. Do not harden.** Print the audit's compromise output. Recommend rebuild + secret rotation. |

Look for `@@ AUDIT_COMPROMISE` in the output as the definitive signal.
Do not proceed past `@@ AUDIT_COMPROMISE` under any circumstances.

## Step 4 — get user approval, then run the hardener

(preflight.sh stage 4 already verified `authorized_keys` for the
trusted user. If you want to check additional users beyond `$USER`
before hardening — e.g., a service account — re-run preflight with
`--users user1 user2 ...` and confirm all relevant users show
`@@ PREFLIGHT_KEY status=OK`. lockdown-ssh.sh requires only one
trusted user with keys, but the broader the coverage the safer.)


Show the user a summary of:
- What was found in the audit.
- Which mode you'll use: `existing` for in-use servers (default),
  `new` for fresh provisions.
- That `--mode existing` will prompt before each destructive step.
- That if SSH breaks during hardening, the orchestrator auto-rolls back
  via the still-alive ControlMaster.

Wait for an explicit "yes, proceed". Then:

```bash
./remote/remote-harden.sh --profile $PROFILE --mode existing
```

(Or `--mode new` for a freshly-provisioned VPS — strict path.)

Watch for these markers and act on them:

| Marker | Action |
|---|---|
| `@@ STAGE_START name=<stage>` | Note which stage. |
| `@@ STAGE_END name=<stage> rc=0` | Continue. |
| `@@ STAGE_END name=<stage> rc=<n>` (non-zero on critical stage) | Investigate; ask user. |
| `@@ VERIFY_OK stage=<stage>` | SSH still works after the stage. Continue. |
| `@@ VERIFY_FAIL stage=<stage>` | SSH broke. Rollback in progress. Stop your guidance. |
| `@@ ROLLBACK_OK stage=<stage>` | Rollback restored SSH. **Stop the run.** Tell the user. Do not try further hardening. |
| `@@ ROLLBACK_FAIL stage=<stage>` | Unrecoverable from here. Tell the user to use their VPS provider's console / VNC. |
| `@@ RUN_DONE log_dir=...` | All stages complete. Proceed to step 6. |
| `@@ RUN_ABORT reason=...` | Run aborted. Print reason, stop. |

## Step 5 — final verification

```bash
vps-proxy run --profile $PROFILE "sudo ufw status verbose"
vps-proxy run --profile $PROFILE "sudo fail2ban-client status sshd"
vps-proxy run --profile $PROFILE "sudo sshd -T | grep -Ei 'passwordauth|permitrootlogin|pubkey'"
vps-proxy run --profile $PROFILE "sysctl kernel.yama.ptrace_scope"
```

Then **ask the user** to test SSH from a brand-new terminal:

```bash
ssh <ssh-host>
```

Do not declare done until they confirm.

## Hard rules

- **Don't** run `audit-persistence.sh --fix` or `audit-app-secrets.sh
  --fix` through automation. Those need human eyes per file.
- **Don't** enable Tailscale + remove public SSH in the same pass. The
  user must verify Tailscale SSH works in a separate session before
  the public rule is dropped.
- **Don't** pass `--force` to anything unless the user explicitly asked.
- **Don't** proceed past `@@ AUDIT_COMPROMISE` under any circumstances.
- **Don't** touch SSH keys, passwords, private key material, or
  `~/.ssh/config` (except via `setup-control-master.sh`).
- **Don't** auto-delete suspicious binaries flagged by
  `audit-persistence.sh` — recommend rebuild instead.

## Recovery (if you locked the host out)

1. Tell the user immediately. Do not try further commands.
2. Point them at their VPS provider's web console / VNC / serial console.
3. Backups for the run are at `/var/backups/vps-hardening/<UTC-timestamp>/`
   on the remote. Restoring from there + restarting `ssh` / `ufw` /
   `fail2ban` undoes everything.
4. Once the user has access back, run `remote-audit.sh` to confirm state.
