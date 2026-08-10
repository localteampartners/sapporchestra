# RUNBOOK — sapporchestra

<!-- UPDATE WHEN: how to run / build / release changes -->

## Build & test (local)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release   # sibling ../sappsounds required
cmake --build build -j8                           # Standalone/VST3/AU/CLI/tests
./build/SappOrchestraTests                        # 38 cases
(cd build && ctest)                               # unit + headless station suite
./verify.sh                                       # full loop, PLUGIN INCLUDED
```

`verify.sh` builds the plugin target on purpose (sappkeys#1: tests green while
the installed binary stayed stale). Never verify with
`-DSAPPORCHESTRA_BUILD_PLUGIN=OFF`.

macOS plugins auto-copy to ~/Library/Audio/Plug-Ins. macOS builds are LOCAL
VERIFICATION ONLY — releases ship Windows artifacts (suite rule). UI/behavior
verification: `SappOrchestraUiShot [--sounds|--orchestra|--cctest|--sfztest DIR]
[out.png]` renders the editor offscreen.

## Headless / station operation (no editor, no user)

The station host never opens the plugin editor and never pumps a JUCE message
loop. Everything below works in that environment.

**Rebuild the SFZ index** — the editor's rescan can never run there, so pick
one (issue #1):

```bash
# 1. The plugin rebuilds its own index at instantiation, for one run of the host:
SAPP_SFZ_RESCAN=1 SAPP_SFZ_ROOT=/path/to/Samples <your host command>

# 2. Or ahead of time, with either shipped binary (both are in the release zip):
sapporchestra-headless index --rescan --root /path/to/Samples
sapporchestra sfz-index --rescan --root /path/to/Samples   # also prints the table
```

The index is `<root>/.sapp-sfz-index.json`, UTF-8 with no BOM. A MISSING index
is scanned and written automatically at the first instantiation; the env var
is only needed when the sample tree CHANGED after the index was written.

**Select an instrument and know it landed:**

- Write the `instrument` host parameter by display name (contract:
  `instrumentSelect` in the SappLink manifest).
- Poll the read-only `libraryReady` parameter instead of guessing a settle
  window: it drops to 0 the instant a selection is written and returns to 1
  when that instrument is installed.
- The plugin names what it is doing in the host log (and in
  `$SAPP_ORCHESTRA_LOG` when that names a file):

```
SappOrchestra-build: version=0.9.0 root="D:\Samples" instruments=1873
SappOrchestra-instrument: loaded slot=0 source="D:\Samples\...\1st Violins Sustain.sfz" build=0.9.0
SappOrchestra-audio-source: instrument="D:\Samples\...\1st Violins Sustain.sfz" name="1st Violins" slot=0 build=0.9.0 ready=1
```

An `instrument="DIAGNOSTIC(...)"` audio-source line means the built-in default
is what sounded — the failure this logging exists to expose.

**Reproduce a station render locally:**

```bash
sapporchestra-headless render --root /path/to/Samples --settle 4000 \
  --instrument "Sonatina Symphonic Orchestra/Strings - Notation/1st Violins Sustain" \
  --out A.wav
sapporchestra-headless selftest        # the #1 + #2 regression suite
```

## Release (VST builds attach automatically, every time)

1. Bump `project(SappOrchestra VERSION X.Y.Z)` in CMakeLists.txt — the
   in-plugin updater compares this (JucePlugin_VersionString) against the
   latest GitHub tag, so the two MUST stay in sync.
2. Update _project/CHANGELOG.md, commit, push.
3. `gh release create vX.Y.Z <local-arm64-zip> --title ... --notes ...`

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
