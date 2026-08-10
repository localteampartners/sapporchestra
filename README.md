# SappOrchestra

<!-- UPDATE WHEN: the one-line description changes, or the repo's top-level layout changes -->

A professional orchestral sample instrument — JUCE **Standalone / VST3 / AU** —
built on the [SappSounds](https://github.com/localteampartners/sappsounds)
engine. SappOrchestra turns modest local SFZ libraries (Virtual Playing
Orchestra, VSCO CE, Sonatina, your own instruments) into a coherent,
expressive performance instrument:

- **Articulations first-class** — keyswitch chips in the UI, an automatable
  articulation parameter, live keyswitch coloring on the keyboard
- **CC1 dynamics ≠ CC11 expression** — dynamics shape level *and* timbre
  (pp is quiet and dark, ff full and bright); expression shapes phrase volume
- **Stage placement** — an XY stage pad (position × depth) driving pan,
  distance damping, predelay, and room sends
- **Coherent space** — early reflections for proximity + a shared 8-line FDN
  hall for one believable room
- **Analog DNA** — subtle per-note ensemble detune, gentle drift, optional
  vintage character; deterministic per seed
- **Built-in sound** — a generated "Diagnostic Orchestra" (sustain, staccato,
  pizzicato) plays instantly with zero sample libraries installed
- **An agent API** — the `sapporchestra` CLI speaks JSON for MIDI-generation
  software (see below)

```text
SappOrchestra  (this repo: orchestra policy, JUCE plugin/standalone, UI, CLI)
      │  links Sapp::Sounds
      ▼
SappSounds     (sibling repo: SFZ, samples, voices, mapping — framework-free)
```

## Build

Check out both repos as siblings, then:

```bash
git clone https://github.com/localteampartners/sappsounds.git
git clone https://github.com/localteampartners/sapporchestra.git
cd sapporchestra
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8          # Standalone, VST3, AU, CLI, tests
./build/SappOrchestraTests
```

The plugin copies itself into `~/Library/Audio/Plug-Ins` on macOS after a
build. `./verify.sh` runs the full loop, plugin target included — don't verify
with `-DSAPPORCHESTRA_BUILD_PLUGIN=OFF`, the headless regression needs the real
processor. (JUCE 8.0.15 and Catch2 are fetched automatically.)

## Headless / unattended use

No editor, no user, no message loop — the way a radio station runs it:

```bash
# Rebuild the SFZ index after adding libraries (the GUI rescan can't run here)
SAPP_SFZ_RESCAN=1 <your host command>              # plugin rebuilds it itself
./build/sapporchestra-headless index --rescan --root /path/to/Samples
./build/sapporchestra sfz-index --rescan --root /path/to/Samples

# Drive a selection the way a host does, and hear the result
./build/sapporchestra-headless render --instrument "Sonatina .../1st Violins Sustain" \
    --root /path/to/Samples --out A.wav
```

Select instruments with the `instrument` host parameter (by display name),
poll the read-only `libraryReady` parameter to know when the load landed, and
grep the host log for `SappOrchestra-audio-source:` lines — they name the
instrument that actually sounded. Details in
[_project/RUNBOOK.md](_project/RUNBOOK.md).

## The agent CLI

Built for MIDI-generating software: every command prints one JSON document.

```bash
# What can this instrument do? (ranges, articulations, keyswitch protocol)
./build/sapporchestra inspect --sfz Violin.sfz

# Is this SFZ usable? (errors, warnings, missing samples, unsupported opcodes)
./build/sapporchestra validate --sfz Violin.sfz

# What parameters exist? (names, ranges, defaults, docs)
./build/sapporchestra params

# Deterministic render through the full orchestra chain:
./build/sapporchestra render --sfz Violin.sfz --midi phrase.mid --out take.wav \
    --param dynamics=0.8 --param stage_x=-0.3 --param hall_decay=3.2 --seed 42
```

Full contract: [docs/agent_api.md](docs/agent_api.md). A three-stem demo
composition script lives at [scripts/make_demo.py](scripts/make_demo.py).

## Docs

- [architecture.md](architecture.md) — design, and how it layers on SappSounds
- [docs/agent_api.md](docs/agent_api.md) — the machine interface
- [docs/boundary_report.md](docs/boundary_report.md) — SappSounds/SappOrchestra
  responsibility split + dependency report
- [`_project/`](_project/) — working state, decisions, roadmap
  (agents: read [CLAUDE.md](CLAUDE.md) first)

## Where releases are built

Tags are built by a **self-hosted GitHub Actions runner on the Windows
machine** (`desktop-14886fp`), not by GitHub's hosted runners — hosted minutes
are billed and the account is currently blocked. Windows jobs read:

```yaml
runs-on: ${{ vars.WINDOWS_RUNNER || 'windows-latest' }}
```

so the repo variable `WINDOWS_RUNNER=self-hosted` sends builds to that
machine, and deleting the variable sends them back to GitHub. No workflow
edits either way.

**Every repo needs its own runner.** The account is a GitHub *user*, not an
organisation, and user accounts can't share runners across repos — so each
repo gets its own registration (its own folder and Windows service) on the
same machine. The prerequisites are installed once and shared: Git, CMake
3.24+, and Visual Studio 2022 Build Tools with the "Desktop development with
C++" workload.

Full setup, including the per-repo registration steps:
[sapptune/RUNNER.md](https://github.com/localteampartners/sapptune/blob/master/RUNNER.md).

**Builds are Windows-only** — macOS jobs were removed on 2026-08-08.
