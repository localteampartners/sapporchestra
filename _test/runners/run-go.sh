#!/usr/bin/env bash
# run-go.sh — thin adapter around `go test ./...`.
# Usage: bash _test/runners/run-go.sh [project-root]   (default: .)
# Normalized exit codes (shared by all runners):
#   0  tests ran and passed
#   1  tests ran, at least one failed (build failures surface here too —
#      read the output to tell them apart)
#   2  not applicable — no go.mod, or module has no test files
#   3  environment error — go toolchain missing
# Bash 3.2 compatible.

set -u
cd "${1:-.}" || exit 3

[ -f go.mod ] || exit 2
command -v go >/dev/null 2>&1 || exit 3

# No *_test.go anywhere → not applicable.
if ! find . -name '*_test.go' -not -path './vendor/*' 2>/dev/null | grep -q .; then
  exit 2
fi

go test ./...
RC=$?
[ "$RC" -eq 0 ] && exit 0
exit 1
