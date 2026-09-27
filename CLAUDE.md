# Non-Visual Melee

An accessibility fork of [melee-pc](https://github.com/999sian/melee-pc), the PC port of Super Smash Bros. Melee. Goal: make the game playable by blind players through screen reader speech (native menus, port menu, launcher) and audio cues in combat. A hobby project: decent code quality, no production ceremony.

Use the vocabulary in `CONTEXT.md` (base port, decomp layer, port layer, hook, announcement, cue, string table, native menu / port menu / launcher). The base port is young and changes fast; every rule below exists to keep base merges cheap.

## Remotes and branches

- `origin` is this fork (`ohylli/non-visual-melee`). `upstream` is the base port, fetch-only: its push URL is disabled on purpose.
- `master` is the fork's own branch. Topic branches hold uncertain work. A `dev` branch gets added once a first alpha is released from `master`.
- A base merge is `git fetch upstream` then `git merge upstream/master` on `master`, conflicts resolved locally; the `/base-merge` skill runs it and writes the digest of upstream changes. `upstream/master` is the base port's only live branch; `upstream/main` and `upstream/testing` are stale.
- All GitHub activity targets this fork. Pull requests, issues and pushes toward the base port happen only when the maintainer asks for that one explicitly. GitHub's "Sync fork" button stays unused: it can discard the fork's commits.
- Fixes unrelated to accessibility are welcome on `master`, kept in their own commits.

## The two layers

- Decomp layer: `src/melee/`, `src/sysdolphin/`, `src/Runtime`, `src/sdk_include`, `src/thp`. Copied from doldecomp/melee, pinned by `src/UPSTREAM_COMMIT`, refreshed by large decomp syncs. Leave its formatting and structure exactly as found; many functions have address-style names (`lbAudioAx_800237A8`) because they are not yet understood.
- Port layer: `src/pc/` (C11 and C++20), `extern/aurora` (vendored graphics, input and UI compatibility library; UI toolkit is RmlUi), `resources/`, `tools/`, `cmake/`.
- The decomp layer is unlicensed Nintendo/HAL code; the port layer is GPL-3.0-or-later, and so is the fork's code. The game needs the player's own disc image, which stays out of git.

## The isolation rule

Base merges conflict wherever the fork edits a base port file, so the fork's footprint there is one-line hooks and nothing else.

- All fork code lives in `src/pc/a11y/`: C++20 internals, built through the fork-owned `src/pc/a11y/a11y.cmake`, which the root `CMakeLists.txt` includes with a single line. Third-party wiring (the Prism screen reader library) goes in that file too.
- A hook is one call to a `pc_a11y_*` function, placed at the semantic transition ("cursor moved", "match started"), the same idiom the base port uses for `pc_is_unlock_all_enabled()`. Fork code decides what to say. Every hook function is declared in the single C-compatible header `src/pc/a11y/a11y_hooks.h`, so that header plus `grep -rn a11y src CMakeLists.txt --exclude-dir=a11y` (the hook calls, the header include and the cmake include) is the full inventory of the fork's footprint. Beyond hooks, the footprint holds screen-reader-only fixes to base port files, so far only the launcher's event pumping while the file dialog is open (`src/pc/launcher.cpp`): the maintainer keeps such fixes in the fork and takes the merge conflicts rather than open upstream pull requests, and marks each with an `a11y` comment so the grep finds it. A fix that helps every player is not footprint: it stays unmarked in its own commit, and `git log --no-merges upstream/master..master -- src ':!src/pc/a11y'` lists it among the fork's other commits to base port files.
- Prefer a hook at the transition over polling game state each frame; poll only where a screen would otherwise need many hooks.
- Native menus draw most labels as images, so spoken text comes from fork-owned string tables keyed by the game's own identifiers.
- Compose each announcement as one text (setting name and value together).
- Accessibility is always compiled in and switched at runtime, default on. Hooks stay free of `#ifdef`.
- Fork settings live in fork-owned storage, read and written by fork code. They are meant to appear in the port menu eventually; revisit storage when that feature is specced.
- Cues use the game's own audio path where possible (`lbAudioAx_800237A8(sfx_id, volume, pan)` plays any game sound with pan; the software mixer is `src/pc/audio.c`).

## Online compatibility

Fork builds are meant to play online against base port builds of the same release; untested so far, since base port online play is itself unverified on Windows.

- Two copies pair when their protocol version, app version string and disc image id match. The fork keeps the base port's version string (`MELEE_APP_VERSION`, `src/pc/version.cpp`), including in fork releases.
- Rollback (online netcode that re-runs recent frames when a late input arrives) needs both machines to simulate identical frames, so fork code reads game state and leaves the simulation untouched.
- Speech is gated in fork code: check `pc_net_resim()` in `hooks.cpp` and stay silent while it is true, so a re-run frame never repeats an announcement.
- Cues are gated like speech and started as audio-private sounds. In a netplay session every sound start returns a handle built from the frame and its place in the frame's call order (`src/pc/net_sfx.h`), and the game keeps those handles as state, so a cue counted in that order would give every later game sound a different handle than on a base port peer. Wrap each cue in `net_sfx_private(true)` / `net_sfx_private(false)`, which leaves it out of the count. A private start plays on every re-run, so a cue also stays silent while `pc_net_resim()` is true.

## Build and run (Windows)

Windows is the only supported target for fork work; keep code portable in principle by confining platform specifics to the screen reader bridge (see `docs/a11y/speech.md`).

- Toolchain: MSYS2 UCRT64 (GCC, CMake, Ninja) with its `bin` directory on `PATH`. GCC is required by the decomp layer.
- Configure once with `cmake -B build -G Ninja`, then `cmake --build build`. The result is `build/melee.exe`, with its DLLs and `resources/` copied beside it. Every decomp file compiles through a Python wrapper that marks rollback-snapshot sections, so a full rebuild takes a while; the link ends with a "Rollback sections" verification line.
- On Windows the game writes `melee-pc.log` in the working directory, not beside `melee.exe`; `MELEE_LOG_FILE` overrides the path.
- The first configure downloads the pinned Prism release into `build/prism-<tag>/` and needs network access; later configures reuse it.
- The base port documents itself in `docs/building.md`, `docs/testing.md`, `docs/debugging.md`, `docs/architecture.md` and `docs/porting-notes.md`. Read the relevant one before exploring the code.

## Verification

The players cannot see the screen, the agent cannot hear the game or drive its menus. The speech log bridges that.

- Every announcement and cue is also written through `pc_log_line` with an `[a11y]` prefix into the log, interleaved with game events. This logging is a developer setting, default on.
- The agent's own check after a change: it builds, and a bounded run exits cleanly and logs the expected `[a11y]` lines. A bounded run boots straight into a scene and exits after a fixed frame count:
  `SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0 SDL_AUDIO_DRIVER=dummy MELEE_BOOT_SCENE=<title|vs|classic|training> MELEE_EXIT_AFTER_FRAMES=<n> MELEE_LOG_FILE=<path> build/melee.exe --no-card <disc>`
  Capture stdout too: the `boot scene:` progress lines go there, not to the log file. The `vs` scene reaches the match within 600 frames; the evidence is `boot scene: state 1 scene 2` in stdout (the fight scene was entered), and the other scenes' evidence lines are in the `SCENES` table of `tools/smoke_test.py`. A `title` run's expected `[a11y]` lines are `speech backend: <name>` and `speak interrupt: "Non-Visual Melee ready"`, ending with `speech shutdown`. A `Device lost ... Device was destroyed` warning at exit is normal shutdown. `MELEE_BOOT_SCENE` also accepts `unranked`, `direct` and `ranked`, which boot into the online lobby and need a peer. Add `MELEE_NO_ATTRACT=1` to a `title` run longer than 600 frames, otherwise the title screen drops into the attract-mode demo fight.
- Add `MELEE_DEBUG_VS=cpu4` to a `vs` run for four CPU fighters (Link, Mario, Fox, Donkey Kong) fighting with no input, the busy scene for checking combat cues. That match is a time match with no clock, so it never ends on its own. `MELEE_DEBUG_VS_STOCKS=<n>` gives each fighter n lives instead, so a run can reach a fighter losing its last life and the match ending on GAME!. `MELEE_DEBUG_VS_STAGE=<n>` picks the stage by its internal stage number (3 is Pokémon Stadium) instead of the last one played. `MELEE_FPS=1` logs one frame-rate line per second.
- `SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0` keeps the game window from taking focus away from a running screen reader, `SDL_AUDIO_DRIVER=dummy` sends the game's audio to SDL's silent driver so the run makes no sound (the mixer still runs; `MELEE_AUDIO_DUMP=<file>` captures the mix if it needs inspecting), and `--no-card` keeps the run off the real memory card. Use all three on every agent-started run. A run with speech on talks through the maintainer's screen reader and interrupts it; add `MELEE_A11Y=0` when the run is not checking speech itself (the log still records each announcement, marked `(off)`). Screenshots are fine when they help.
- To check native menus, drive them with `tools/a11y/drive.py`: it runs the game on a script of controller presses (the base port's `MELEE_KEY_FIFO` input driver, fed over a named pipe, so the window never needs focus), waits for output lines, takes screenshots of the window, and prints the `[a11y]` lines between the steps. Its `--help` lists the script steps; `tools/a11y/main_menu.drive` gets from the title screen to the main menu, the start of most menu scripts.
- The port menu and the launcher are out of the driver's reach; they depend on the maintainer play-testing, after which the agent reads the `[a11y]` lines against what they report. "Works" is reserved for what a blind tester has confirmed by ear, on any screen.
- Base port tests: `cmake --build build --target unit_tests` then `ctest --test-dir build -L melee`. On Windows the suite is `launcher_data`, `version`, `endian`, `thp` and the fork's `speech` and `launcher_speech`; the netplay tests are Linux-only. `launcher_data` currently fails on Windows, a base port bug in the test's file handling (it can pass on a first run). `tools/smoke_test.py` runs on Windows only as `MELEE_BIN=<full path to melee.exe> python tools/smoke_test.py --no-disc`; its disc cases isolate save data on Linux only and would touch the real memory card here.
- A debug control server (state queries, pause, input injection over localhost) is a possible future direction, not a commitment.

## Style and lint

Fork code follows `CODING_STYLE.md` at the repo root. Before committing, run `python tools/check_style.py` (clang-format, banned bare `long`, whitespace; covers `src/pc/` only). It needs `clang-format` on `PATH`.

## Docs

The code is the source of truth for what the fork does. A doc records what stays true as features land and what the code does not reveal without real digging: invariants, reasons, constraints from outside systems (Prism, rollback, the decomp), gotchas.

- Primers that explain a subsystem in plain terms ("how native menus work", "the audio path") go in `docs/a11y/`, written as the knowledge is gained, each opening with a short mental model. They name files and functions and leave the details to the code: no line numbers, current call sites, log line formats or test lists. A short illustrative snippet is fine.
- Native menu work starts from `docs/a11y/native-menus.md`: how the game's menus run, where their words come from, a screen inventory and candidate hook points.
- A reason or procedure tied to one spot in the code is a comment at that spot.
- A decision with its rejected alternatives is an ADR in `docs/adr/`.
- Specs and issues under `.scratch/` are snapshots of the plan when written. Later work supersedes them, and the code wins where they disagree.
- The maintainer navigates docs by heading with a screen reader, so a clear heading structure matters more than length.

`docs/*` is ignored by the base port; only `docs/agents/`, `docs/adr/` and `docs/a11y/` are excepted for the fork.

## Accessibility status

Add one line per feature as it lands: what it does, where it lives.

- Speech (`src/pc/a11y/`, primer `docs/a11y/speech.md`): the subsystem every feature hands announcements to, speaking through Prism, with the proof-of-life announcement "Non-Visual Melee ready" at startup. Switches `MELEE_A11Y` and `MELEE_A11Y_LOG`. Play-tested by ear with NVDA and the Windows voice fallback; whether speech resumes after an NVDA restart is still untested.
- Launcher (`src/pc/a11y/launcher_speech.cpp` and `rmlui_reader.cpp`, primer `docs/a11y/rmlui-screens.md`): an RmlUi plugin and the `pc_a11y_launcher_frame` hook read the launcher page aloud: the opening, Home and update banner buttons, the disc on Play, button label changes, the status line with settle and progress rules, and Tab as Down. Settings reads tabs with their control-less rows, settings with name, value and help, sliders, text fields and the Settings status lines. Play-tested by ear with NVDA. Open: reading page text line by line (issue 03) and text field editing (issue 04).

## Agent skills

### Issue tracker

Issues and specs live as local markdown files under `.scratch/<feature-slug>/`. See `docs/agents/issue-tracker.md`.

### Triage labels

The five default triage roles (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`), recorded as a `Status:` line in each issue file. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
