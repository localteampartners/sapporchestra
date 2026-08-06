#!/usr/bin/env bash
# .claude/hooks/doc-nudge.sh — Stop hook for sappcode-scaffolded projects.
#
# Reminds Claude when the working tree has source-file changes but no
# _project/*.md updates this session. Non-blocking; stays silent on clean
# trees, tiny changes (< 3 source files), and turns where _project/ was
# already touched.
#
# Triggered automatically via .claude/settings.json Stop hooks.

set -u
cd "${CLAUDE_PROJECT_DIR:-$(pwd)}" 2>/dev/null || exit 0
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || exit 0

# Noise to ignore in the change set
EXCLUDE_RE='\.claude/|node_modules/|^dist/|^build/|\.tsbuildinfo$|package-lock\.json$|yarn\.lock$|pnpm-lock\.yaml$|_project/STATUS\.md$'

# git status --porcelain: 'XY path' lines. Strip the leading 3-char status.
changed=$(git status --porcelain 2>/dev/null | awk '{ $1=""; sub(/^ +/, ""); print }' | grep -vE "$EXCLUDE_RE" || true)
[ -z "$changed" ] && exit 0

src=$(echo "$changed" | grep -vE '^_project/' | grep -c . || true)
doc=$(echo "$changed" | grep -E '^_project/' | grep -c . || true)

if [ "$src" -ge 3 ] && [ "$doc" -eq 0 ]; then
  echo "🔔 $src source files changed but no _project/ updates this session — see CLAUDE.md 'Keep the docs current' table for which file(s) to touch (CURRENT_STATE, CHANGELOG, DECISIONS, etc.)."
fi

exit 0
