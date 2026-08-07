# RUNBOOK — sapporchestra

<!-- UPDATE WHEN: how to run / build / release changes -->

## Build & test (local)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # sibling ../sappsounds required
cmake --build build -j8                           # Standalone/VST3/AU/CLI/tests
./build/SappOrchestraTests                        # 22 cases
./verify.sh                                       # fast loop (plugin off)
```

macOS plugins auto-copy to ~/Library/Audio/Plug-Ins. UI verification:
`SappOrchestraUiShot [--sounds|--orchestra|--cctest] [out.png]` renders the
editor offscreen (—orchestra loads the full 16-channel Sonatina preset first).

## Release (VST builds attach automatically, every time)

```bash
gh release create vX.Y.Z <local-arm64-zip> --title ... --notes ...
```

Creating the release pushes the tag, which triggers
`.github/workflows/release-builds.yml`: macOS-universal and Windows-x64 zips
(VST3 + Standalone + CLI + INSTALL.txt) build on CI and attach to the same
release. Manual re-run: Actions → release-builds → Run workflow with the tag.

## Samples

In-plugin: GET SOUNDS (downloads to the shared samples folder; FULL
ORCHESTRA preset needs Sonatina). CLI/new machine:
`../sappsounds/scripts/fetch-library.sh get all`.

## Rollback

Releases are additive; delete a bad release/tag with `gh release delete` +
`git push --delete origin <tag>`. Plugin state schema is versioned
(stateVersion 2, legacy sfzPath migrates to slot 1).
