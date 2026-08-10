#!/usr/bin/env bash
# verify.sh — fast feedback loop for sapporchestra.
#
# The PLUGIN is built here on purpose. sappkeys#1's postmortem: the tests were
# green while the installed binary stayed stale, because verification skipped
# the plugin target. The headless regression (sapporchestra #1/#2) also runs
# the real processor, so it only exists when the plugin is built.
# First run pulls JUCE and takes a few minutes; after that it is incremental.

set -e
set -o pipefail   # a failing test must fail the script, not just print
cd "$(dirname "$0")"

echo "▶ configure"
if [ ! -d build ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release > /dev/null
fi

echo "▶ build (core + cli + tests + plugin + headless harness)"
cmake --build build -j8 2>&1 | grep -E "error|FAILED" && exit 1 || true

echo "▶ tests"
./build/SappOrchestraTests --reporter compact 2>&1 | grep -E "passed|failed" | tail -1

echo "▶ headless station regression (no GUI, no message loop)"
./build/SappOrchestraHeadless_artefacts/Release/sapporchestra-headless selftest \
  2>/dev/null | grep -E "FAIL|selftest:"

echo "▶ cli smoke"
./build/sapporchestra params > /dev/null
# Against a COPY: sfz-index writes an index, and the fixture must stay clean.
smoke_root="$(mktemp -d)/sfz-headless"
cp -R tests/data/sfz-headless "$smoke_root"
./build/sapporchestra sfz-index --rescan --root "$smoke_root" > /dev/null
rm -rf "$smoke_root"
./build/sapporchestra inspect --diagnostic | head -c 120; echo " ..."

echo "✓ verify passed"
