#!/usr/bin/env bash
# verify.sh — fast feedback loop for sapporchestra.
#
# Goal: finish in under 60 seconds. Runs typecheck + lint + tests.
# Called by /ship, and any time Claude (or you) wants to confirm the
# project is healthy. If it's slow, this loop stops being useful.
#
# Fill in the checks appropriate for your stack. Uncomment what applies
# and delete the rest.

set -e

echo "▶ typecheck"
# npm run typecheck
# pnpm exec tsc --noEmit
# uv run mypy .
# go vet ./...
# cargo check --all-targets

echo "▶ lint"
# npm run lint
# uv run ruff check .
# golangci-lint run
# cargo clippy --all-targets -- -D warnings

echo "▶ tests"
# npm test
# uv run pytest -q
# go test ./...
# cargo test

echo "✓ verify passed"
