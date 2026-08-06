# Playbook — source code

## What "backed up" means here

Every commit on every branch is reachable from **at least 2 git
remotes**, **at least 1 of them off your laptop**. GitHub's `origin`
counts as one. Acceptable second copies (any one):

- A second remote (Codeberg, GitLab, a self-hosted Forgejo, etc.)
- A clone on a separate machine you control (server, second laptop)
- A documented acceptance in `_project/DECISIONS.md` that GitHub-only
  is enough — valid, but record it explicitly so you've made the call

Working state that isn't committed and pushed isn't backed up at all.

## Verification steps

1. **Remotes exist and point somewhere.**

   ```bash
   git remote -v
   ```

   - At least one named `origin`. If none, that's a hard gap.
   - The URL should look like a remote you control (github.com,
     codeberg.org, your own host).

2. **Working tree is clean** (anything dirty isn't backed up).

   ```bash
   git status --short
   ```

   - Empty output → clean.
   - Untracked or modified files → soft gap. Note them; the user may
     have intentionally-uncommitted scratch work, but flag for review.

3. **Every local commit is on a remote.**

   ```bash
   git fetch --all --quiet
   git log --branches --not --remotes --oneline | head
   ```

   - Empty output → all committed work is on at least origin.
   - Any output → those commits exist only locally. Hard gap.

4. **Every local branch has a remote tracking branch.**

   ```bash
   git for-each-ref --format='%(refname:short) %(upstream:short)' refs/heads/ | awk '$2==""'
   ```

   - Empty output → fine.
   - Any output → those branches exist locally without remote
     counterparts. Soft gap.

5. **A second redundancy exists** (the off-host requirement).

   Inspect `git remote -v` for additional remotes beyond `origin`.
   - 2+ remotes → ok.
   - 1 remote (origin only) → check `_project/DECISIONS.md` for an
     entry like "GitHub-only acceptable" or similar. If present, ok. If
     absent, gap: "no second remote and no documented acceptance."

6. **The deployed commit is reachable** (recovery story).

   If `.monitor.yml` lists a primary URL and the project deploys from a
   git ref:
   - Check `_project/RUNBOOK.md` for the deploy procedure.
   - Confirm the deploy uses a remote ref (e.g. `git pull origin main`),
     not a path that requires the developer's laptop. If it depends on
     local state (rsyncing from `~/code/...`), gap.

## What counts as a gap

| Severity | Condition | Example finding line |
|---|---|---|
| Hard | No origin remote | `[backup] source-code: no git remote configured — set up GitHub origin` |
| Hard | Local commits not on any remote | `[backup] source-code: <N> local commits not pushed to any remote` |
| Hard | Origin only, no documented acceptance | `[backup] source-code: only one remote (origin); add a second or document the acceptance in DECISIONS.md` |
| Soft | Untracked / modified files | `[backup] source-code: working tree dirty (<file count> files) — commit or stash` |
| Soft | Local-only branches | `[backup] source-code: <N> branches without upstream — push or delete` |
| Soft | Deploy depends on local state | `[backup] source-code: RUNBOOK references local path for deploy — switch to remote-pull` |

Record gaps in `_project/TODO.md` per the format in `verify.md`.

## What to put in BACKUP.md

- All hard gaps closed and at least one soft gap remaining → **gaps**
- All gaps closed → **ok**
- Couldn't run the checks (e.g. not a git repo somehow) → **unverified**

Set "last verified" to today.
