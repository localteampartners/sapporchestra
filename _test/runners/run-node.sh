#!/usr/bin/env bash
# run-node.sh — thin adapter around the project's native Node test runner.
# Usage: bash _test/runners/run-node.sh [project-root]   (default: .)
# Normalized exit codes (shared by all runners):
#   0  tests ran and passed
#   1  tests ran, at least one failed
#   2  not applicable — no package.json, or no real test script
#   3  environment error — runtime/package manager missing
# Native output passes through on stdout/stderr; read it for failure names.
# Bash 3.2 compatible.

set -u
cd "${1:-.}" || exit 3

[ -f package.json ] || exit 2
command -v node >/dev/null 2>&1 || exit 3

# A real "test" script must exist and not be npm's default placeholder.
HAS_TEST=$(node -e '
  try {
    var pkg = require(process.cwd() + "/package.json");
    var t = (pkg.scripts || {}).test || "";
    process.stdout.write(!t || /no test specified/i.test(t) ? "no" : "yes");
  } catch (e) { process.stdout.write("err"); }
' 2>/dev/null)
[ "$HAS_TEST" = "err" ] && exit 3
[ "$HAS_TEST" = "yes" ] || exit 2

# Pick the package manager the lockfile implies; fall back to npm.
PM=npm
[ -f pnpm-lock.yaml ] && PM=pnpm
[ -f yarn.lock ] && PM=yarn
command -v "$PM" >/dev/null 2>&1 || PM=npm
command -v "$PM" >/dev/null 2>&1 || exit 3

if [ "$PM" = "yarn" ]; then
  "$PM" test
else
  "$PM" test --silent
fi
RC=$?
[ "$RC" -eq 0 ] && exit 0
exit 1
