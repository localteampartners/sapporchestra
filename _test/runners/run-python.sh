#!/usr/bin/env bash
# run-python.sh — thin adapter around pytest (fallback: unittest discover).
# Usage: bash _test/runners/run-python.sh [project-root]   (default: .)
# Normalized exit codes (shared by all runners):
#   0  tests ran and passed
#   1  tests ran, at least one failed
#   2  not applicable — no Python project markers, or no tests collected
#   3  environment error — interpreter missing or runner crashed
# Native output passes through; read it for failure names.
# Bash 3.2 compatible.

set -u
cd "${1:-.}" || exit 3

if [ ! -f pyproject.toml ] && [ ! -f setup.py ] && [ ! -f setup.cfg ] \
   && [ ! -f requirements.txt ] && [ ! -d tests ] && [ ! -d test ]; then
  exit 2
fi

PY=python3
command -v "$PY" >/dev/null 2>&1 || PY=python
command -v "$PY" >/dev/null 2>&1 || exit 3

if "$PY" -c "import pytest" >/dev/null 2>&1; then
  "$PY" -m pytest -q
  RC=$?
  # pytest: 0 pass · 1 failures · 5 no tests collected · 2/3/4 env-ish
  case "$RC" in
    0) exit 0 ;;
    1) exit 1 ;;
    5) exit 2 ;;
    *) exit 3 ;;
  esac
fi

# No pytest — fall back to stdlib discovery if a test dir exists.
if [ -d tests ] || [ -d test ]; then
  "$PY" -m unittest discover
  RC=$?
  [ "$RC" -eq 0 ] && exit 0
  # Python 3.12+: exit 5 means no tests found
  [ "$RC" -eq 5 ] && exit 2
  exit 1
fi

exit 2
