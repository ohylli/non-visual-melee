# Non-Visual Melee

An accessibility fork of [melee-pc](https://github.com/999sian/melee-pc), the PC port of Super Smash Bros. Melee. Goal: make the game playable by blind players through screen reader speech (native menus, port menu, launcher) and audio cues in combat. A hobby project: decent code quality, no production ceremony.

Use the vocabulary in `CONTEXT.md` (base port, decomp layer, port layer, hook, announcement, cue, string table, native menu / port menu / launcher). The base port is young and changes fast; every rule below exists to keep base merges cheap.

## Remotes and branches

- `origin` is this fork (`ohylli/non-visual-melee`). `upstream` is the base port, fetch-only: its push URL is disabled on purpose.
- `master` is the fork's own branch. Topic branches hold uncertain work. A `dev` branch gets added once a first alpha is released from `master`.
- A base merge is `git fetch upstream` then `git merge upstream/master` on `master`, conflicts resolved locally. `upstream/master` is the base port's only live branch; `upstream/main` and `upstream/testing` are stale.
- All GitHub activity targets this fork. Pull requests, issues and pushes toward the base port happen only when the maintainer asks for that one explicitly. GitHub's "Sync fork" button stays unused: it can discard the fork's commits.
- Fixes unrelated to accessibility are welcome on `master`, kept in their own commits.

## The two layers

- Decomp layer: `src/melee/`, `src/sysdolphin/`, `src/Runtime`, `src/sdk_include`, `src/thp`. Copied from doldecomp/melee, pinned by `src/UPSTREAM_COMMIT`, refreshed by large decomp syncs. Leave its formatting and structure exactly as found; many functions have address-style names (`lbAudioAx_800237A8`) because they are not yet understood.
- Port layer: `src/pc/` (C11 and C++20), `extern/aurora` (vendored graphics, input and UI compatibility library; UI toolkit is RmlUi), `resources/`, `tools/`, `cmake/`.
- The decomp layer is unlicensed Nintendo/HAL code; the port layer is GPL-3.0-or-later, and so is the fork's code. The game needs the player's own disc image, which stays out of git.

## The isolation rule

Base merges conflict wherever the fork edits a base port file, so the fork's footprint there is one-line hooks and nothing else.

- All fork code lives in `src/pc/a11y/`: C++20 internals, built through the fork-owned `src/pc/a11y/a11y.cmake`, which the root `CMakeLists.txt` includes with a single line. Third-party wiring (the Prism screen reader library) goes in that file too.
- A hook is one call to a `pc_a11y_*` function, placed at the semantic transition ("cursor moved", "match started"), the same idiom the base port uses for `pc_is_unlock_all_enabled()`. Fork code decides what to say. Every hook function is declared in the single C-compatible header `src/pc/a11y/a11y_hooks.h`, so that header plus `grep -rn pc_a11y_ src --exclude-dir=a11y` is the full inventory of the fork's footprint.
- Prefer a hook at the transition over polling game state each frame; poll only where a screen would otherwise need many hooks.
- Native menus draw most labels as images, so spoken text comes from fork-owned string tables keyed by the game's own identifiers.
- Compose each announcement as one text (setting name and value together).
- Accessibility is always compiled in and switched at runtime, default on. Hooks stay free of `#ifdef`.
- Fork settings live in fork-owned storage, read and written by fork code. They are meant to appear in the port menu eventually; revisit storage when that feature is specced.
- Cues use the game's own audio path where possible (`lbAudioAx_800237A8(sfx_id, volume, pan)` plays any game sound with pan; the software mixer is `src/pc/audio.c`).

## Build and run (Windows)

Windows is the only supported target for fork work; keep code portable in principle by confining platform specifics to the speech wrapper.

- Toolchain: MSYS2 UCRT64 (GCC, CMake, Ninja) with its `bin` directory on `PATH`. GCC is required by the decomp layer.
- Configure once with `cmake -B build -G Ninja`, then `cmake --build build`. The result is `build/melee.exe`, with its DLLs and `resources/` copied beside it.
- The game writes `melee-pc.log` into its working directory; `MELEE_LOG_FILE` overrides the path.

## Verification

There is no test suite for game behaviour, the players cannot see the screen, and the agent cannot hear the game. The speech log bridges that.

- Every announcement and cue is also written through `pc_log_line` with an `[a11y]` prefix into `melee-pc.log`, interleaved with game events. This logging is a developer setting, default on.
- The agent cannot drive the game, so its own check after a change is: it builds, and the game still launches. Report that as "builds and launches"; "works" is reserved for what a blind tester has confirmed by ear. After the maintainer play-tests, read the `[a11y]` lines in the log against what they report.
- When launching the game, start it minimized and in the background so it never takes focus from a running screen reader. Screenshots are fine when they help.
- A debug control server (state queries, pause, input injection over localhost) is a possible future direction, not a commitment.

## Style and lint

Fork code follows `docs/CODING_STYLE.md`. Before committing, run `python tools/check_style.py` (clang-format, banned bare `long`, whitespace; covers `src/pc/` only). It needs `clang-format` on `PATH`.

## Docs

Primers that explain a subsystem in plain terms ("how native menus work", "the audio path") go in `docs/a11y/`, written as the knowledge is gained, each opening with a short mental model. `docs/*` is ignored by the base port; only `docs/agents/`, `docs/adr/` and `docs/a11y/` are excepted for the fork.

## Accessibility status

Nothing shipped yet. Add one line per feature as it lands: what it does, where it lives.

## Agent skills

### Issue tracker

Issues and specs live as local markdown files under `.scratch/<feature-slug>/`. See `docs/agents/issue-tracker.md`.

### Triage labels

The five default triage roles (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`), recorded as a `Status:` line in each issue file. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
