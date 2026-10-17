# Non-Visual Melee

An accessibility fork of [melee-pc](https://github.com/999sian/melee-pc), the PC port of Super Smash Bros. Melee. Goal: make the game playable by blind players through screen reader speech (native menus, port menu, launcher) and audio cues in combat. A hobby project: decent code quality, no production ceremony.

Use the vocabulary in `CONTEXT.md` (base port, decomp layer, port layer, hook, announcement, cue, string table, native menu / port menu / launcher). The base port is young and changes fast; every rule below exists to keep base merges cheap.

## Remotes and branches

- `origin` is this fork (`ohylli/non-visual-melee`). `upstream` is the base port, fetch-only: its push URL is disabled on purpose.
- `master` is the fork's own branch. Topic branches hold uncertain work. A `dev` branch gets added once a first alpha is released from `master`.
- A base merge is `git fetch upstream` then `git merge upstream/master` on `master`, conflicts resolved locally; the `/base-merge` skill runs it and writes the digest of upstream changes. `upstream/master` is the base port's only live branch; `upstream/main` and `upstream/testing` are stale.
- All GitHub activity targets this fork. Pull requests, issues and pushes toward the base port happen only when the maintainer asks for that one explicitly. GitHub's "Sync fork" button stays unused: it can discard the fork's commits.
- Fixes unrelated to accessibility are welcome on `master`, kept in their own commits.
- CI is the fork-owned `.github/workflows/a11y-windows.yml`: each push to `master` cross-builds the Windows package with the base port's `tools/package_windows.sh` and republishes it as the rolling prerelease `ci-latest`. The base port's workflows stay in the tree but are disabled in the fork's GitHub settings (`gh workflow disable`); a base merge that adds a workflow file disables it the same way before pushing.

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
- Cues play through the cue engine: the fork's own SDL audio stream beside the game's, filled by miniaudio, with the fork's own constant-power pan and start and stop ramps (ADR-0005). A cue that should be a game sound may use the game's path instead (`lbAudioAx_800237A8(sfx_id, volume, pan)` plays any game sound the current screen has loaded, with pan; the software mixer is `src/pc/audio.c`).

## Online compatibility

Fork builds are meant to play online against fork builds and base port builds of the same release; the `verify` skill's online level checks both over loopback (primer `docs/a11y/netplay.md`).

- Two copies pair when their protocol version, app version string and disc image id match. The fork keeps the base port's version string (`MELEE_APP_VERSION`, `src/pc/version.cpp`), including in fork releases.
- Rollback (online netcode that re-runs recent frames when a late input arrives) needs both machines to simulate identical frames, so fork code reads game state and leaves the simulation untouched.
- The one place the fork writes is the pad hook, `pc_a11y_pad` in `publish_locked` (`src/pc/keyboard.c`): steering adds its stick to port 1's virtual pad there, before the game and the netplay code sample it, so the peer receives it as a player's input (ADR-0004). Nothing else the fork does may change game state or input.
- Floor tests call the game's own raycast, `mpCheckFloor`, from the tick hook only: it sets temporary marks on the stage's collision data and clears them before returning, so between ticks it leaves nothing changed (ADR-0006).
- Speech is gated in fork code: check `pc_net_resim()` in `hooks.cpp` and stay silent while it is true, so a re-run frame never repeats an announcement.
- Cues are gated like speech: silent while `pc_net_resim()` is true. A cue on the cue engine never reaches the game's sounds. A cue through the game's path is started as an audio-private sound: in a netplay session every sound start returns a handle built from the frame and its place in the frame's call order (`src/pc/net_sfx.h`), and the game keeps those handles as state, so a cue counted in that order would give every later game sound a different handle than on a base port peer. Wrap each such cue in `net_sfx_private(true)` / `net_sfx_private(false)`, which leaves it out of the count; a private start plays on every re-run, which the gate silences.

## Build and run (Windows)

Windows is the only supported target for fork work; keep code portable in principle by confining platform specifics to the screen reader bridge (see `docs/a11y/speech.md`).

- Toolchain: MSYS2 UCRT64 (GCC, CMake, Ninja, and libusb from the `mingw-w64-ucrt-x86_64-libusb` package) with its `bin` directory on `PATH`. GCC is required by the decomp layer.
- Configure once with `cmake -B build -G Ninja -DAURORA_SDL3_PROVIDER=vendor -DLibUSB_LIBRARY=<msys64>/ucrt64/lib/libusb-1.0.a -DLibUSB_INCLUDE_PATH=<msys64>/ucrt64/include/libusb-1.0`, then `cmake --build build`. The first flag builds SDL (the library under windows, input and audio) from source at the commit aurora pins, the same SDL the base port's release packages ship; without it aurora uses an older prebuilt SDL on Windows. SDL built that way needs libusb for GameCube controller adapters, and the configure stops without it; the two libusb flags name the static library, so libusb goes inside `SDL3.dll` as in the base port's Windows release instead of needing `libusb-1.0.dll` beside it. The settings stay in the build directory's cache, so only a fresh build directory needs them again. The result is `build/melee.exe`, with its DLLs and `resources/` copied beside it. Every decomp file compiles through a Python wrapper that marks rollback-snapshot sections, so a full rebuild takes a while; the link ends with a "Rollback sections" verification line.
- On Windows the game writes `melee-pc.log` in the working directory, not beside `melee.exe`; `MELEE_LOG_FILE` overrides the path.
- The first configure downloads the pinned Prism release into `build/prism-<tag>/` and SDL's source, and needs network access; later configures reuse them until a base merge moves SDL's pinned commit.
- The base port documents itself in `docs/building.md`, `docs/testing.md`, `docs/debugging.md`, `docs/architecture.md` and `docs/porting-notes.md`. Read the relevant one before exploring the code.

## Driving the game

`tools/a11y/drive.py` is the agent's way around the game: it runs the game on a script of controller presses (the base port's `MELEE_KEY_FIFO` input driver, fed over a named pipe, so the window never needs focus), waits for output lines, saves screenshots of the window, and prints the `[a11y]` lines between the steps. Its `--help` lists the script steps, and inline steps (`-c`) suit a quick look. `tools/a11y/main_menu.drive` gets from the title screen to the main menu, the start of most scripts.

- Reach for it whenever you need to know what a screen does: exploring a menu before planning its speech, screenshotting a screen that is not yet accessible to read its layout and labels, reproducing a bug, and verification.
- It sets `MELEE_A11Y_STEER=1`, which turns steering on with speech off; a run of the game started any other way needs it too for the D-pad to steer.
- Every agent-started game run stays silent (`drive.py` and `drive_net.py` see to it) and leaves the window focus and the real memory card alone: `SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0 SDL_AUDIO_DRIVER=dummy` and `--no-card`, plus `MELEE_A11Y=0` unless the run checks speech itself. Focus taken mid-sentence cuts off the maintainer's screen reader.
- `tools/a11y/drive_net.py` does the same for two instances in one netplay session over loopback (primer `docs/a11y/netplay.md`).
- Keep `build/drive` clear: the default transcripts (`last.log`, `net/`, `level/`) are overwritten by each run, and a transcript saved under its own name is removed once its check is done, unless there is a specific reason to keep it, such as a bug still being diagnosed. `build/drive/cache` holds the drivers' shader caches and stays.

## Verification

The players cannot see the screen and the agent cannot hear the game. The speech log bridges that: every announcement and cue is also written into the game's log with an `[a11y]` prefix, interleaved with game events, and the agent checks a change by reading those lines.

- Verify every change with the `verify` skill. It has four levels (quick, menus, online, base merge), from about a minute to about ten; choose the level the change calls for by your own judgement, unless the maintainer names one.
- "Works" is reserved for what a blind tester has confirmed by ear, on any screen. The port menu and the launcher are out of the drivers' reach and depend on the maintainer play-testing.
- A change that adds or changes a verification tool, script, switch, expected log line or known failure updates the `verify` skill in the same change, so the skill stays the one place that says how to verify.

## Style and lint

Fork code follows `CODING_STYLE.md` at the repo root. Before committing, run `python tools/check_style.py` (clang-format, banned bare `long`, whitespace; covers `src/pc/` only). It needs `clang-format` on `PATH`.

## Docs

The code is the source of truth for what the fork does. A doc records what stays true as features land and what the code does not reveal without real digging: invariants, reasons, constraints from outside systems (Prism, rollback, the decomp), gotchas.

- Primers that explain a subsystem in plain terms ("how native menus work", "the audio path") go in `docs/a11y/`, written as the knowledge is gained, each opening with a short mental model. They name files and functions and leave the details to the code: no line numbers, current call sites, log line formats or test lists. A short illustrative snippet is fine.
- Native menu work starts from `docs/a11y/native-menus.md`: how the game's menus run, where their words come from, a screen inventory and candidate hook points.
- Match work starts from `docs/a11y/gameplay-state.md`: what the game knows during a match (damage, stocks, positions, the stage's lines, blast zones, items), how to read it safely, and candidate hook points.
- A reason or procedure tied to one spot in the code is a comment at that spot.
- Tunable values (settings' defaults, timings and levels set by ear) live only in the code. Docs, the `verify` skill and the status lines below name a setting and what it does, not its value or a count that follows from it, so tuning one changes code, tests and drive scripts alone.
- A decision with its rejected alternatives is an ADR in `docs/adr/`.
- Specs and issues under `.scratch/` are snapshots of the plan when written. Later work supersedes them, and the code wins where they disagree.
- The maintainer navigates docs by heading with a screen reader, so a clear heading structure matters more than length.

`docs/*` is ignored by the base port; only `docs/agents/`, `docs/adr/` and `docs/a11y/` are excepted for the fork.

## Accessibility status

Add one line per feature as it lands: what it does and where it lives (its main files, hooks by name, spec and primer). Every feature is play-tested by ear with NVDA unless its line names what still waits, with the issue's path. The words spoken live in the code, the drive scripts in the `verify` skill, and the hooks' call sites in the grep above.

- Speech (`src/pc/a11y/`, primer `docs/a11y/speech.md`): the subsystem every feature hands announcements to, speaking through Prism, with a proof-of-life announcement at startup. Switches `MELEE_A11Y` and `MELEE_A11Y_LOG`.
- Cue engine (`src/pc/a11y/cue_engine.cpp`, `cue_output.cpp` and `cue_sounds.cpp`, the `pc_a11y_master_volume` hook, ADR-0005, primer `docs/a11y/cues.md`): the second SDL stream every cue plays through, with miniaudio for sounds: looped and pulsed voices, ramps, constant-power pan and the volume chain. Switch `MELEE_A11Y_CUES`, records `MELEE_A11Y_CUE_DUMP` and `MELEE_A11Y_CUE_TRACE`, analysis `tools/a11y/cue_dump.py`.
- Match cues (`src/pc/a11y/match_cues.cpp` with a file per cue, opened with the cue settings by `cue_system.cpp`, the `pc_a11y_match_tick` hook, spec `.scratch/match-cues/spec.md`, primer `docs/a11y/cues.md`): polls the match once a tick. Your position cue pans and pitches with where you are and quickens as the nearest opponent nears; the opponent's position cue says where the nearest opponent is; the gap warning hums while no floor is below you; the ledge warning pulses on the side of a near edge of the floor below you. Each cue's settings are in `a11y-cues.ini` in the base port's settings folder, re-read while the game runs (`MELEE_A11Y_CUE_SETTINGS` names another file). Waiting: the position cues' pan on stages other than Battlefield and Final Destination, and every cue online (`.scratch/match-cues/issues/11-play-test-online.md`).
- Launcher (`src/pc/a11y/launcher_speech.cpp` and `rmlui_reader.cpp`, the `pc_a11y_launcher_frame` hook, primer `docs/a11y/rmlui-screens.md`): an RmlUi plugin reads the launcher's Home and Settings pages aloud: buttons, status lines, tabs, settings with value and help, sliders and text fields. Open: reading page text line by line (`.scratch/launcher-accessibility/issues/03-reading-text-line-by-line.md`) and text field editing (`.scratch/launcher-accessibility/issues/04-text-field-editing.md`).
- Scene announcements (`src/pc/a11y/scene_speech.cpp`, the `pc_a11y_scene_entered` hook): names the scene the player arrives in, once it has loaded, from a table keyed by `GameSceneKind`, with what to press where the screen has no reader yet. Matches and the memory card scene stay silent, and screens with their own reader speak their own opening.
- Main menu tree (`src/pc/a11y/menu_speech.cpp` and `menu_names.cpp`, the `pc_a11y_menu_frame` and `pc_a11y_menu_center_text` hooks, ADR-0003): polls the tree once a frame and says each screen's title, the hovered entry and its description. Sound, Screen display and Multi-Man Melee read their rows and values; the other leaf screens say their name and "No speech yet." Names from a fork string table, descriptions decoded from the game's text.
- Rules screens (`src/pc/a11y/menu_speech.cpp` and `menu_names.cpp`, the `pc_a11y_stage_switch_refused` hook, spec `.scratch/rules-screens/spec.md`): Custom Rules, Additional Rules, Item Switch and Random Stage say each row with its value from `GameRules`, and each change. The main menu tree's reader serves them in both their hosts, the main menu tree and character select's rules header. Online waits for `.scratch/rules-screens/issues/05-play-test-online.md`.
- Character select steering (`src/pc/a11y/css_reader.cpp` and `css_targets.cpp` on the shared `steering.cpp`, `steerer.hpp` and `steps.hpp`, the `pc_a11y_pad` and `pc_a11y_css_coin` hooks, ADR-0004): the D-pad steps the local hand over rows of targets (the top bar, the portraits, the player slots' buttons and knobs, and in single-player modes the level and stock arrows), naming each destination at the press while a glide of generated stick input takes the hand there. Single-player modes steer only when controller 1 started the mode. Open findings: `.scratch/character-select/issues/` 10 to 15.
- Character select speech (`src/pc/a11y/css_speech.cpp`, `css_names.cpp` and `css_targets.cpp`, the `pc_a11y_css_frame` and `pc_a11y_css_hand` hooks, spec `.scratch/character-select/spec.md`): follows the local player's hand in VS and single-player modes: the opening, each portrait a carried coin enters, the coin picked up or put back, the costume, each button and knob a free hand reaches with its value, slot, team and rule changes, the rules header, and online other players' changes. The choice itself is left to the game's announcer.
- Stage select speech and steering (`src/pc/a11y/sss_reader.cpp`, `sss_speech.cpp`, `sss_names.cpp` and `sss_targets.cpp`, the `pc_a11y_sss_frame`, `pc_a11y_sss_resolved` and `pc_a11y_sss_cursor` hooks, spec `.scratch/stage-select/spec.md`): says each tile's stage with a short description, Random's roll and locked tiles, and the stage when the rules skip the screen; the D-pad steps the stage cursor over the tiles as drawn, with glides. Online each player steers their own cursor, and once both have picked, the coin flip's result is said from the player's own side. Online and the Turns and Loser rules wait for `.scratch/stage-select/issues/08-play-test-multiplayer.md`.
- LAN play (`src/pc/a11y/lobby_speech.cpp`, the `pc_a11y_lobby_frame` and `pc_a11y_chat_buttons` hooks, spec `.scratch/lan-play/spec.md`, primer `docs/a11y/netplay.md`): polls the LAN lobby's view once a frame and says the machines listed, found and leaving, connecting, starting, a failure with the base port's reason, and the return to the lobby. Other online lobbies say "No speech yet." Quick chat leaves the D-pad to steering on character select.
- Training menu (`src/pc/a11y/training_speech.cpp` and `training_names.cpp`, item names shared with Item Switch in `item_names.cpp`, read from the `pc_a11y_match_tick` hook, spec `.scratch/training-menu/spec.md`, primer `docs/a11y/native-menus.md`): says the menu's opening with the hovered row and its value, the keys on a session's first opening, each row change with its value and each value change alone; a quick run of changes is said once, at the value it settles on.

## Agent skills

### Issue tracker

Issues and specs live as local markdown files under `.scratch/<feature-slug>/`. See `docs/agents/issue-tracker.md`.

### Triage labels

The five default triage roles (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`), recorded as a `Status:` line in each issue file. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.
