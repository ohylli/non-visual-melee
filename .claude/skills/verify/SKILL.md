---
name: verify
description: Verify a change to the fork at the level it calls for: build, style, unit tests, bounded runs, menu drive scripts and online runs, with the commands and what each pass looks like. Use after a change, before committing, and after a base merge.
---

The agent cannot hear the game, so verification reads the game's log: every announcement and cue is also written through `pc_log_line` with an `[a11y]` prefix, interleaved with game events (a developer setting, default on). A step passes when the game exits cleanly and the expected `[a11y]` lines are there.

## 1. Choose a level

Four levels, each including the one before it. Choose the one the change calls for by your own judgement, unless the maintainer names one; when torn between two, take the higher. Times are from one sequential run on the maintainer's machine. Nearly all of it is the game running scripts in real time, and a failed wait adds its timeout.

1. **Quick** (about 1 minute), after any change and while iterating: the build, the style check, the unit tests, a bounded run of the scene the change touches (`title` when none fits), and the drive scripts covering the screen or feature the change touched.
2. **Menus** (about 4 minutes more), after a change to menu speech or to code every screen shares (`hooks.cpp`, the speech subsystem), and before committing a menu feature: all nine menu drive scripts.
3. **Online** (about 4 minutes more), after a change to steering, the pad hook, cues, the online lobby, or anything that could reach game state, input or sound handles: the five `css_net.drive` runs and the `lan_lobby.drive` run.
4. **Base merge** (about 4 minutes more): the five `css_net.drive` runs and the `lan_lobby.drive` run again against a base port build, alongside the `base-merge` skill's own checks.

## 2. Run it

Run the steps one at a time: game instances started together compete for the input driver and the window. Done means every step of the level passed, or each failure is a known one below or a flake that passed on an unchanged rerun.

## 3. Report

Name the level, then each step's result, known failures and reruns included. "Works" is reserved for what a blind tester has confirmed by ear, on any screen; a passing level shows the log says the right things.

## Every game run

`SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0` keeps the game window from taking focus away from a running screen reader, `SDL_AUDIO_DRIVER=dummy` sends the game's audio to SDL's silent driver so the run makes no sound (the mixer still runs; `MELEE_AUDIO_DUMP=<file>` captures the mix if it needs inspecting), and `--no-card` keeps the run off the real memory card (the base port then builds a save with everything unlocked, but All-Star and Sound Test appear in the main menu tree only once Start is pressed on the title screen, not from the save itself). Use all three on every agent-started run. A run with speech on talks through the maintainer's screen reader and interrupts it; add `MELEE_A11Y=0` when the run is not checking speech itself (the log still records each announcement, marked `(off)`). Screenshots are fine when they help.

## Build, style and unit tests

- Build: `cmake --build build`.
- Style: `python tools/check_style.py` (needs `clang-format` on `PATH`; covers `src/pc/` only).
- Unit tests: `cmake --build build --target unit_tests` then `ctest --test-dir build -L melee`. On Windows the suite is `launcher_data`, `version`, `endian`, `thp` and the fork's own suites, each an `add_test` in `src/pc/a11y/a11y.cmake`; the netplay tests are Linux-only. `launcher_data` currently fails on Windows, a base port bug in the test's file handling (it can pass on a first run).
- `tools/smoke_test.py` runs on Windows only as `MELEE_BIN=<full path to melee.exe> python tools/smoke_test.py --no-disc`; its disc cases isolate save data on Linux only and would touch the real memory card here.

## Bounded runs

A bounded run boots straight into a scene and exits after a fixed frame count:

`SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0 SDL_AUDIO_DRIVER=dummy MELEE_BOOT_SCENE=<title|vs|classic|training> MELEE_EXIT_AFTER_FRAMES=<n> MELEE_LOG_FILE=<path> build/melee.exe --no-card <disc>`

- Capture stdout too: the `boot scene:` progress lines go there, not to the log file.
- The `vs` scene reaches the match within 600 frames; the evidence is `boot scene: state 1 scene 2` in stdout (the fight scene was entered). The other scenes' evidence lines are in the `SCENES` table of `tools/smoke_test.py`.
- A `title` run's expected `[a11y]` lines are `speech backend: <name>`, `speak interrupt: "Non-Visual Melee ready"` and `speak interrupt: "Title screen. Press Start."`, ending with `speech shutdown`. Add `MELEE_NO_ATTRACT=1` to a `title` run longer than 600 frames, otherwise the title screen drops into the attract-mode demo fight.
- A `Device lost ... Device was destroyed` warning at exit is normal shutdown.
- `MELEE_BOOT_SCENE` also accepts `unranked`, `direct` and `ranked`, which boot into the online lobby and need a peer.
- Add `MELEE_DEBUG_VS=cpu4` to a `vs` run for four CPU fighters (Link, Mario, Fox, Donkey Kong) fighting with no input, the busy scene for checking combat cues. That match is a time match with no clock, so it never ends on its own. `MELEE_DEBUG_VS_STOCKS=<n>` gives each fighter n lives instead, so a run can reach a fighter losing its last life and the match ending on GAME!. `MELEE_DEBUG_VS_STAGE=<n>` picks the stage by its internal stage number (3 is Pokémon Stadium) instead of the last one played. `MELEE_FPS=1` logs one frame-rate line per second.

## Menu drive scripts

`tools/a11y/drive.py` is described under "Driving the game" in `CLAUDE.md`. Run a script as `python tools/a11y/drive.py tools/a11y/<name>.drive`; its transcript goes to `build/drive/last.log` unless `--out` names another. The nine menu drive scripts each fail on a missing announcement:

- `tree_walk.drive`: every tree screen of the main menu tree, a leaf screen and a return from character select.
- `leaf_screens.drive`: the leaf screens with a reader.
- `css_portraits.drive`: character select's portraits and coin, moving the hand with the stick.
- `css_slots.drive`: its player slots, CPU level slider, Ready to Fight, Teams button and rules screen.
- `css_steps.drive`: D-pad steps over the portraits and their glides.
- `css_setup.drive`: a whole VS setup by D-pad and buttons, from choosing a character to the match beginning.
- `css_classic.drive`: Classic's character select by D-pad: a character, the level arrows both ways and at the lowest level, the stock arrow, and Start into the stage intro.
- `css_training.drive`: Training's character select by D-pad: a character for the player, the CPU's coin found and carried to another portrait, and the match beginning.
- `results.drive`: results after a one-stock debug match the player walks out of, its announcement, and two Starts after the announcer leaving it. It needs the debug match switches: `MELEE_DEBUG_VS=cpu MELEE_DEBUG_VS_STOCKS=1 MELEE_DEBUG_VS_STAGE=31 python tools/a11y/drive.py tools/a11y/results.drive`.

Flakes: a step that logs "no pad change seen for that press" lost its press to the input driver; rerun before suspecting speech. A press can also land a frame off with no such line (a cursor back where it was); a script that fails once and passes on an unchanged rerun lost a press too.

## Online runs

`tools/a11y/drive_net.py` runs two instances of the game in one netplay session over loopback (see "Driving the game" in `CLAUDE.md`), from one script whose steps are addressed to instance a (player 1) or b (player 2); each is a `drive.py` run with its own key pipe, log and recording of controller data. A session starts at boot, unless `--lan` leaves it to the LAN lobby, and walks the ordinary menus in sync, so a's presses move both. After the script it fails on a missing session, a `net: DESYNC` in either log, or recordings that differ over the frames both recorded, and it lists each side's delay and every glide's frames. Primer: `docs/a11y/netplay.md`.

`tools/a11y/css_net.drive` steps player 1 over the portraits into its player slot while player 2 chooses by stick, each hearing the other's choice, and starts the match. Its five runs, below; each writes to `build/drive/net` unless given its own `--out-dir`, so give each one to keep all five:

```
python tools/a11y/drive_net.py --delay 0 tools/a11y/css_net.drive
python tools/a11y/drive_net.py --delay 2 tools/a11y/css_net.drive
python tools/a11y/drive_net.py --delay 6 tools/a11y/css_net.drive
python tools/a11y/drive_net.py --delay 10 tools/a11y/css_net.drive
python tools/a11y/drive_net.py --sim-delay-ms 100 tools/a11y/css_net.drive
```

`tools/a11y/lan_lobby.drive` starts both instances with no session: each walks the menus into the LAN lobby on its own presses, they find each other there, and Start on a takes both into character select. Holding B there takes both back to the lobby; Start again, then closing a alone (`a quit`), returns b to its lobby with the connection lost. It fails on a missing lobby announcement: the opening, the other machine found, starting, and the two returns. It runs with `--lan`, which skips the recording comparison (`docs/a11y/netplay.md` says why):

```
python tools/a11y/drive_net.py --lan --out-dir build/drive/lan tools/a11y/lan_lobby.drive
```

The LAN run needs a network interface up (Ethernet or Wi-Fi with an address; `lan: no usable network interface` in a log means none) and a Windows Firewall rule letting that build's `melee.exe` take UDP in. A build directory without a rule makes the first run prompt and take focus, so ask the maintainer to allow it first.

At the base merge level, add `--base-b <dir>` to each, instance b then being a base port build of the merged commit from that build directory: build it in a git worktree outside the repo with its own build directory, with the fork's native Windows build fix applied (`docs/a11y/netplay.md`).

## Out of reach

The port menu and the launcher are out of the driver's reach; they depend on the maintainer play-testing, after which the agent reads the `[a11y]` lines against what they report. A debug control server (state queries, pause, input injection over localhost) is a possible future direction, not a commitment.
