# Playbook — VPS config

## What "backed up" means here

The customized files on the VPS — sshd config, nginx sites, systemd
units, fail2ban rules, ufw rules, scripts in `/usr/local/bin`, anything
under `/etc/` you've changed — can be restored from **at least 2
sources**, **at least 1 off-host**:

- Provisioning code in this repo (Ansible, shell scripts, Dockerfiles)
- An off-host backup of the relevant paths (restic, BorgBackup, rsync
  to another host, snapshot of `/etc` to S3)
- A second VPS that mirrors the config (rare for personal projects)

A VPS that exists nowhere else but on the live host is one disaster
away from being unrecoverable.

## When this playbook does NOT apply

If the project has no VPS (`.vps-proxy.json` was removed at init
because the project is SaaS-only / static / serverless), delete this
playbook and remove the row from BACKUP.md. The driver `verify.md`
skips missing playbooks.

## Verification steps

The checks run via `vps-proxy run` so they hit the project's actual
VPS profile. If `.vps-proxy.json` is missing, stop and tell the user
this playbook doesn't apply.

1. **Profile is set.**

   ```bash
   test -f .vps-proxy.json && cat .vps-proxy.json
   ```

   - Profile name visible → continue.
   - Otherwise → playbook doesn't apply (see above).

2. **Inventory customized paths.**

   The standard suspects:

   ```bash
   vps-proxy run "ls -1 /etc/nginx/sites-enabled/ 2>/dev/null"
   vps-proxy run "systemctl list-unit-files --state=enabled --no-pager 2>/dev/null | head -30"
   vps-proxy run "ls -1 /etc/systemd/system/*.service 2>/dev/null"
   vps-proxy run "ls -1 /usr/local/bin/ 2>/dev/null"
   vps-proxy run "test -f /etc/fail2ban/jail.local && echo 'fail2ban customized'"
   vps-proxy run "ufw status 2>/dev/null | head -10"
   ```

   Read the output. The set of customized files for this project
   should be small enough to enumerate. If `_project/INFRASTRUCTURE.md`
   has a "Customized paths" list, cross-check against what's actually
   on the host — drift is a soft gap.

3. **Sshd hardening matches sappsecurevps's audit.**

   If sappsecurevps was run on this project (`/harden-vps` history),
   the relevant settings should match its expectations. Spot-check:

   ```bash
   vps-proxy run "grep -E '^(PermitRootLogin|PasswordAuthentication|PubkeyAuthentication)' /etc/ssh/sshd_config"
   ```

   Findings here aren't backup-status per se, but if the VPS got
   re-imaged from a vanilla image, you'd lose this hardening — that's
   the backup question.

4. **Provisioning code, if any, is in git.**

   Look for: `provision/`, `ansible/`, `playbooks/`, `infra/`, a
   `Dockerfile` that builds the deployable, a `docker-compose.yml`,
   `.github/workflows/` deploy actions.

   - Found → ok, that's source #1.
   - None of these → soft gap. The VPS config is undocumented in code.
     You can still pass with off-host backup (source #2), but it's
     fragile.

5. **Off-host config backup exists.** (The load-bearing question.)

   Claude can't see this — ask the user:

   > Is `/etc/` (or at least the customized paths from step 2) backed
   > up to anywhere off this VPS? If yes, what tool and what
   > destination?

   Acceptable answers (all real-world):
   - "restic to B2/S3, hourly /etc snapshots"
   - "rsync to another VPS nightly"
   - "a tarball in 1Password I update by hand each time I change
     something" (not great cadence-wise but it's a copy)
   - "the provisioning code is the backup" (accept if step 4 was ok
     and the code is reproducible)

   Unacceptable:
   - "no" → hard gap.
   - "I think so?" → ask them to verify and record.

6. **Restore plan is documented in RUNBOOK.md.**

   If a fire happened tomorrow, what's the procedure? Look for a
   "Restore from disaster" or "Rebuild from scratch" section in
   `_project/RUNBOOK.md`. Ideally with: target VPS provider, base
   image, list of paths to restore, sequence.

   - Section exists → ok.
   - Missing → soft gap.

## What counts as a gap

| Severity | Condition | Example finding line |
|---|---|---|
| Hard | No off-host config backup AND no provisioning code | `[backup] vps-config: customized paths exist nowhere but on the live host` |
| Hard | No `.vps-proxy.json` but RUNBOOK references VPS | `[backup] vps-config: project has VPS state but no profile to access it` |
| Soft | INFRASTRUCTURE.md drift from actual customized paths | `[backup] vps-config: <N> paths on VPS not listed in INFRASTRUCTURE.md` |
| Soft | No provisioning code | `[backup] vps-config: no provisioning code — config exists only as live state + backups` |
| Soft | No restore section in RUNBOOK | `[backup] vps-config: RUNBOOK has no restore-from-scratch procedure` |

## What to put in BACKUP.md

- Hard gaps unresolved → **gaps**
- Hard closed, soft may remain → **ok**
- VPS unreachable / vps-proxy missing → **unverified**
