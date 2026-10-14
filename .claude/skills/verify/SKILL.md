---
name: verify
description: Verify a change to the fork at the level it calls for: build, style, unit tests, bounded runs, menu drive scripts and online runs, with the commands and what each pass looks like. Use after a change, before committing, and after a base merge.
---

The agent cannot hear the game, so verification reads the game's log: every announcement and cue is also written through `pc_log_line` with an `[a11y]` prefix, interleaved with game events (a developer setting, default on). A step passes when the game exits cleanly and the expected `[a11y]` lines are there.

## 1. Choose a level

Four levels, each including the one before it. Choose the one the change calls for by your own judgement, unless the maintainer names one; when torn between two, take the higher. Times are from the maintainer's machine, the drive scripts run side by side by `tools/a11y/run_level.py`. Nearly all of it is the game running scripts in real time, and a failed wait adds its timeout.

1. **Quick** (about 1 minute), after any change and while iterating: the build, the style check, the unit tests, a bounded run of the scene the change touches (`title` when none fits), and the drive scripts covering the screen or feature the change touched (a match cue: the match drive scripts below, `run_level.py match_position match_opponent`).
2. **Menus** (about 1 minute more), after a change to menu speech or to code every screen shares (`hooks.cpp`, the speech subsystem), and before committing a menu feature: all sixteen menu drive scripts, `run_level.py menus`.
3. **Online** (about 40 seconds more), after a change to steering, the pad hook, cues, the online lobby, or anything that could reach game state, input or sound handles: the three `css_net.drive` runs and the `lan_lobby.drive` run, `run_level.py online`.
4. **Base merge** (about 40 seconds more): the online runs again against a base port build, `run_level.py online --base-b <dir>`, alongside the `base-merge` skill's own checks.

After a change to cues or the match tick hook, export `MELEE_A11Y_CUES=1` for every scripted run of the level: without it the cue engine stays closed and the tick hook returns at once, so the runs would not exercise the change.

## 2. Run it

`python tools/a11y/run_level.py` runs drive scripts side by side and reports each: `menus` and `online` name the levels' sets, script names (`css_steps sss_hover`) the quick level's, in one call (`run_level.py menus online`). Its `--help` has the rest. Games running at once each need their own shader cache slot (below) and UDP ports, which the runner hands out; start no other game run beside it. Done means every step of the level passed, or each failure is a known one below or a flake that passed on an unchanged rerun: `run_level.py <name>` reruns one by the name its line printed, an online run included (`run_level.py lan_lobby --base-b <dir>`, the same `--base-b` as the level's).

## 3. Report

Name the level, then each step's result, known failures and reruns included. "Works" is reserved for what a blind tester has confirmed by ear, on any screen; a passing level shows the log says the right things.

## Every game run

`SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0` keeps the game window from taking focus away from a running screen reader, `SDL_AUDIO_DRIVER=dummy` sends the game's audio to SDL's silent driver so the run makes no sound (the mixer still runs; `MELEE_AUDIO_DUMP=<file>` captures the mix if it needs inspecting), and `--no-card` keeps the run off the real memory card (the base port then builds a save with everything unlocked, but All-Star and Sound Test appear in the main menu tree only once Start is pressed on the title screen, not from the save itself). Use all three on every agent-started run. `drive.py` and `drive_net.py` keep the game's shader caches in `build/drive/cache/<slot>` (`--cache-slot`, default 1; `drive_net.py --cache-slots`, default 1 and 2), made on first use as a copy of the maintainer's, about 120 MB each: two games sharing one log `database is locked` and run without it. A run with cues on (below) also reads and writes the cue settings file, which by default is the maintainer's own in `%APPDATA%\melee-pc`: give a hand-started run, `tools/smoke_test.py` included (its launcher cases leave speech, and so cues, on), `MELEE_A11Y_CUE_SETTINGS=<scratch file>`. Set but empty, it means no file at all (`[a11y] cue settings: MELEE_A11Y_CUE_SETTINGS is empty; using the defaults, no file`); only unset falls back to the maintainer's file. `drive.py` and `drive_net.py` set it themselves (below). A run with speech on talks through the maintainer's screen reader and interrupts it; add `MELEE_A11Y=0` when the run is not checking speech itself (the log still records each announcement, marked `(off)`). Screenshots are fine when they help.

## Build, style and unit tests

- Build: `cmake --build build`.
- Style: `python tools/check_style.py` (needs `clang-format` on `PATH`; covers `src/pc/` only).
- Unit tests: `cmake --build build --target unit_tests` then `ctest --test-dir build -L melee`. `position_cue` tests the position cues' drivers, `match_cues` the match cues with their voices rendered. On Windows the suite is `launcher_data`, `version`, `endian`, `thp`, `updater`, `updater_browser` and the fork's own suites, each an `add_test` in `src/pc/a11y/a11y.cmake`; the netplay tests are Linux-only. `cue_engine` writes `build/cue_pulse_train.f32`, which `cue_dump_analysis` then reads through the dump analysis script (it needs the Python CMake found, standard library only). `launcher_data` currently fails on Windows, a base port bug in the test's file handling (it can pass on a first run).
- `tools/smoke_test.py` runs on Windows only as `MELEE_A11Y_CUE_SETTINGS=<scratch file> MELEE_BIN=<full path to melee.exe> python tools/smoke_test.py --no-disc` (the variable keeps its launcher cases off the maintainer's cue settings); its disc cases isolate save data on Linux only and would touch the real memory card here.

## Bounded runs

A bounded run boots straight into a scene and exits after a fixed frame count:

`SDL_WINDOW_ACTIVATE_WHEN_SHOWN=0 SDL_AUDIO_DRIVER=dummy MELEE_BOOT_SCENE=<title|vs|classic|training> MELEE_EXIT_AFTER_FRAMES=<n> MELEE_LOG_FILE=<path> build/melee.exe --no-card <disc>`

- Capture stdout too: the `boot scene:` progress lines go there, not to the log file.
- The `vs` scene reaches the match within 600 frames; the evidence is `boot scene: state 1 scene 2` in stdout (the fight scene was entered). The other scenes' evidence lines are in the `SCENES` table of `tools/smoke_test.py`.
- A `title` run's expected `[a11y]` lines are `speech backend: <name>`, `speak interrupt: "Non-Visual Melee ready"` and `speak interrupt: "Title screen. Press Start."`, ending with `speech shutdown`. Add `MELEE_NO_ATTRACT=1` to a `title` run longer than 600 frames, otherwise the title screen drops into the attract-mode demo fight.
- A `Device lost ... Device was destroyed` warning at exit is normal shutdown.
- `MELEE_BOOT_SCENE` also accepts `unranked`, `direct` and `ranked`, which boot into the online lobby and need a peer.
- Cues (primer `docs/a11y/cues.md`): the cue engine opens only with `MELEE_A11Y` on, so agent runs add `MELEE_A11Y_CUES=1` (cues on with speech off; `0` keeps them off). Its open line is `[a11y] cue engine: SDL driver dummy, device 44100 Hz, 1024-frame buffer` with the dummy driver; a run that cannot open it logs `[a11y] cue engine could not start: <why>` and `speak queue (off): "Audio cues could not start."` (an unknown `SDL_AUDIO_DRIVER` provokes it). Each voice logs `[a11y] cue <name> start|stop` with its targets. `MELEE_A11Y_CUE_DUMP=<file>` records the cue mix (raw f32 stereo 48 kHz; the dummy driver still pulls it in real time) and `MELEE_A11Y_CUE_TRACE=<file>` every voice's targets each tick, each announced by an `[a11y] cue dump:` or `[a11y] cue trace:` line. `python tools/a11y/cue_dump.py <dump> [--window s] [--start s] [--end s] [--onsets]` reports level, pan, pulse onsets and intervals and pitch per window. `drive.py` and `drive_net.py` pass the environment through, so `MELEE_A11Y_CUES=1` in front of them runs a script with the engine open.
- Cue settings (`docs/a11y/cues.md`, "Cue settings"): as the engine opens, the log names the file (`[a11y] cue settings: file <path>`), and a missing one adds `[a11y] cue settings: written with defaults: <path>`. `drive.py` and `drive_net.py` set `MELEE_A11Y_CUE_SETTINGS` to a file of each run's own, `<out>.cues.ini` beside the transcript (`build/drive/last.log.cues.ini`, `build/drive/net/a.log.cues.ini`), removed first so each run starts from the defaults; a non-empty `MELEE_A11Y_CUE_SETTINGS` in their environment names another file instead, kept as it is, for a run with tuned values. A reload logs each changed value (`[a11y] cue settings: cues.volume = 40`), and a bad line `[a11y] cue settings: line 2 not understood: ...` with `speak queue (off): "Cue settings, line 2 not understood."`. The file is polled only at scene entry and in a match, so a run that edits it sees the change at the next scene. `cue_settings` is the unit test suite.
- Add `MELEE_DEBUG_VS=cpu4` to a `vs` run for four CPU fighters (Link, Mario, Fox, Donkey Kong) fighting with no input, the busy scene for checking combat cues. That match is a time match with no clock, so it never ends on its own. `MELEE_DEBUG_VS_STOCKS=<n>` gives each fighter n lives instead, so a run can reach a fighter losing its last life and the match ending on GAME!. `MELEE_DEBUG_VS_STAGE=<n>` picks the stage by its internal stage number (3 is Pokémon Stadium, 31 Battlefield) instead of Final Destination, the debug match's own. `MELEE_FPS=1` logs one frame-rate line per second.

## Menu drive scripts

`tools/a11y/drive.py` is described under "Driving the game" in `CLAUDE.md`. Run one script as `python tools/a11y/drive.py tools/a11y/<name>.drive`, its transcript in `build/drive/last.log` unless `--out` names another, or several through `run_level.py`, their transcripts in `build/drive/level/<name>.log`. The sixteen menu drive scripts each fail on a missing announcement:

- `tree_walk.drive`: every tree screen of the main menu tree, a leaf screen and a return from character select.
- `leaf_screens.drive`: the leaf screens with a reader.
- `rules_screens.drive`: Custom Rules and Additional Rules from the main menu tree: each row, a value changed and put back on each, Rules to Stock renaming the next row, the returns from Item Switch and Random Stage, and the wrap. Score Display stays hidden in agent runs.
- `rules_switches.drive`: Item Switch and Random Stage from the main menu tree. Item Switch: an item toggled and back, a move to the other column, the frequency strip from the right column with its hint ("Item frequency: Medium. Left and right to adjust.") and from the left without it, the frequency changed and back. Random Stage: every stage turned off but Rainbow Cruise and A on it saying "At least one stage must stay on." About 40 seconds, most of it the 29 stages.
- `css_portraits.drive`: character select's portraits and coin, moving the hand with the stick.
- `css_slots.drive`: its player slots, CPU level slider, Ready to Fight, Teams button and Custom Rules' opening from the rules header.
- `css_rules.drive`: the rules screens from character select's rules header: the header saying the rules ("Rules: 2-minute KO fest!"), Custom Rules' opening with its descriptions read from character select's string table, Rules changed to Stock, Item Switch, Additional Rules and Random Stage each visited and returned from, character select's opening on the way back, the header saying "Rules: 3-man survival test!", a second visit opening with the hint again, and Start back to character select. About 40 seconds.
- `css_steps.drive`: D-pad steps over the portraits and their glides.
- `css_setup.drive`: a whole VS setup by D-pad and buttons, from choosing a character to the match beginning.
- `css_classic.drive`: Classic's character select by D-pad: a character, the level arrows both ways and at the lowest level, the stock arrow, and Start into the stage intro.
- `css_training.drive`: Training's character select by D-pad: a character for the player, the CPU's coin found and carried to another portrait, and the match beginning.
- `sss_hover.drive`: stage select reached from VS mode: its opening, the stage cursor moved with the stick naming the tiles it enters, each stage with its description ("Battlefield. One floating stage ..."), the random tile, and A on it saying the stage it rolled ("Random: Corneria.") before the match begins. `--no-card` unlocks every stage, so it meets no locked tile.
- `sss_skipped.drive`: VS mode's stage-select rule set to Random on Custom Rules, opened from the rules header, then Start on character select: the skipped stage select says the stage chosen in advance ("Stage: Corneria.") before the match begins. The script cannot fail on a line that should be absent, so `run_level.py` fails it on one saying the stage select opening (`"Stage select\.`; the quote and full stop keep Custom Rules' "Stage selection" row out). Run alone, check that `grep -c '"Stage select\.' build/drive/last.log` prints 0.
- `sss_steps.drive`: stage select by D-pad: the first press from the stage cursor's start to Icicle Mountain, along the top row, down a column and back along the second row to Icicle Mountain and Right back to the second row, down through the random tile's row to the bottom one and Down at its edge, then up and along to the random tile, each name at its press and each glide arriving (`glide to "Random" ended after 8 frames: arrived`), and A rolling a stage ("Random: Brinstar.") before the match begins.
- `sss_glide_pick.drive`: A pressed during a glide picks the tile hovered then, not the one the step named: the stick takes the stage cursor from Rainbow Cruise into the bottom left corner, a step Up glides towards Battlefield, and A at once stops the glide (`glide to "Battlefield" ended after 8 frames: steering is off here`) and says "Picked Rainbow Cruise." before the match on Rainbow Cruise begins.
- `results.drive`: results after a one-stock debug match the player walks out of, its announcement, and two Starts after the announcer leaving it. It needs the debug match switches: `MELEE_DEBUG_VS=cpu MELEE_DEBUG_VS_STOCKS=1 MELEE_DEBUG_VS_STAGE=31 python tools/a11y/drive.py tools/a11y/results.drive`.

Flakes: a step that logs "no pad change seen for that press" lost its press to the input driver; rerun before suspecting speech. A press can also land a frame off with no such line (a cursor back where it was); a script that fails once and passes on an unchanged rerun lost a press too. A hang is rarer: `tree_walk.drive` failed once (2026-10-14, the menus level side by side) waiting for character select's opening after A on VS Melee, the game log ending at that press with not even its release, no archive error; it passed on a rerun.

## Match drive scripts

`tools/a11y/match_position.drive` checks your position cue (primer `docs/a11y/cues.md`, "The match cues") in a Training match on Final Destination: Pichu against a standing CPU, about 45 seconds. Run it with cues on and both records:

```
MELEE_A11Y_CUES=1 MELEE_A11Y_CUE_TRACE=build/drive/match.trace MELEE_A11Y_CUE_DUMP=build/drive/match.f32 \
  python tools/a11y/drive.py tools/a11y/match_position.drive
```

- **The log.** Six `[a11y] cue your position` lines, `grep -c '\[a11y\] cue your position st' build/drive/last.log` printing 6: the start as the match begins (`start pan -0.42 pitch 1.06 gain 1.00 interval 0.800`, Pichu on the left, the CPU far off), then stop and start for the Training menu, stop and start for it again with half speed chosen, and a stop when it opens a last time before Finish. The opponent's cue follows each of them with its own line (`cue opponent position start pan 0.42 pitch 1.06 gain 1.00 interval 0.400` for the standing CPU), six in all. Half speed holds the fighters every other tick and must not stop the cue: a seventh line means it did, or that Pichu fell (a stop with `pitch 0.50` below the floor; the script also fails then, waiting for the first pause's stop on the floor). The scene change comes with the cue already stopped, so it logs nothing of its own.
- **The trace.** Pan follows the walk: about -0.42 at the start, -0.57 at the left edge's slide (x = -80.8), past -0.6 off the edge and back, rising through 0 on the run right to about +0.51 beyond the CPU; pitch dips just under 1 below the floor and rises with each jump; the interval falls from 0.800 to about 0.08 as Pichu comes near the CPU. Pan p inverts to x = p / 0.6 × 85.6 on Final Destination's main floor.
- **The dump.** `python tools/a11y/cue_dump.py build/drive/match.f32 --window 0.5 --start 14 --end 31` (the match starts about 14.5 s into the run; take the start from the dump's first non-silent window): the pan column swings from about -0.57 to about +0.51, the interval column falls from 0.800 to about 0.08, windows during the menu are silent, and the half speed windows keep pulsing.

`tools/a11y/match_opponent.drive` checks the opponent's position cue in the debug VS match on Final Destination: Link against a Mario who stands still with no input, two stocks each, about 25 seconds. Link dashes over and forward-smashes Mario off the right edge; the script searches for the knockout and waits for the respawn. Run it with the debug match switches, cues on and both records (`run_level.py match_opponent` sets the switches itself):

```
MELEE_DEBUG_VS=1 MELEE_DEBUG_VS_STOCKS=2 MELEE_A11Y_CUES=1 \
  MELEE_A11Y_CUE_TRACE=build/drive/opponent.trace MELEE_A11Y_CUE_DUMP=build/drive/opponent.f32 \
  python tools/a11y/drive.py tools/a11y/match_opponent.drive
```

- **The log.** Both cues start at GO, `cue your position start pan -0.42 pitch 1.00 gain 1.00 interval 0.800` and `cue opponent position start pan 0.42 pitch 0.99 gain 1.00 interval 0.400`; seven presses of the search knock Mario out, `cue opponent position stop pan 0.65 pitch 0.50` (past the right edge, at the bottom blast zone); a second later he is back on the respawn platform, `cue opponent position start pan -0.35 pitch 1.99`. Your cue logs nothing more: the run quits mid-match.
- **The trace.** The opponent's pan stays at 0.42 until the smashes push Mario right, to about 0.55 near the edge and 0.65 past it, while his pitch falls to 0.50 as he drops; after the respawn it is -0.35, the pitch sinking from about 1.97 as the platform comes down; your interval falls from 0.800 to about 0.10 as Link closes in and goes back to 0.800 once Mario respawns far off.
- **The dump**, one cue at a time by its register (the match starts about 5 s into the dump): `python tools/a11y/cue_dump.py build/drive/opponent.f32 --band 150 1000 --threshold 0.3 --window 1 --start 5 --end 21` shows the opponent's train, every interval 0.400 (one of about 1.0 across the knockout), pan +0.42 rising to +0.65, then -0.34 after the respawn, pitch 400 Hz on the floor; `--band 1100 12000 --threshold 0.3` over the same range shows your tick at 1500 Hz, its interval from 0.800 down to about 0.10 and back to 0.800. Without `--band` the two blur: the opponent's 82 ms pulse hides the ticks that start inside it.

Runs of the other scripts with `MELEE_A11Y_CUES=1` meet the cues too: `css_setup.drive`, `css_classic.drive`, `css_training.drive` and the stage select scripts each log one `cue your position start` and one `cue opponent position start` as their match begins (`sss_skipped.drive` none: it quits before GO), `results.drive` both starts, the stop of its fall and the opponent's stop at GAME!, and each `css_net.drive` instance one start of each, its own player's cue at pan about -0.72 on a and +0.72 on b, and the other player's the other way round.

## Online runs

`tools/a11y/drive_net.py` runs two instances of the game in one netplay session over loopback (see "Driving the game" in `CLAUDE.md`), from one script whose steps are addressed to instance a (player 1) or b (player 2); each is a `drive.py` run with its own key pipe, log and recording of controller data. A session starts at boot, unless `--lan` leaves it to the LAN lobby, and walks the ordinary menus in sync, so a's presses move both. After the script it fails on a missing session, a `net: DESYNC` in either log, or recordings that differ over the frames both recorded, and it lists each side's delay and every glide's frames. Primer: `docs/a11y/netplay.md`.

`tools/a11y/css_net.drive` steps player 1 over the portraits into its player slot while player 2 chooses by stick, each hearing the other's choice. On stage select each steps to a stage by D-pad and picks it (player 1 Princess Peach's Castle, player 2 Kongo Jungle; a base port player 2 presses Start for Random), each hears "Waiting for your opponent." after its own pick, and the coin flip's result from its own side ("Princess Peach's Castle, your pick." on a, "Princess Peach's Castle, opponent's pick." on b), then the match starts. The default seed 7 always lets player 1's pick win; `--seed 4` lets player 2's win ("Kongo Jungle, opponent's pick." or, against a base port build, "Random: Kongo Jungle, opponent's pick." on a), worth a run after a change to the result's words. `run_level.py online` runs it three times side by side: at input delay 0, at delay 10, and over a slow link (`--sim-delay-ms 100`, where the delay changes during the session and rollbacks happen). Each glide takes its delay-0 frames plus the delay, so delays between 0 and 10 would add nothing that the steering unit tests, which glide at every delay from 0 to 10, leave out. One run by hand writes to `build/drive/net` unless `--out-dir` names another:

```
python tools/a11y/drive_net.py --delay 10 tools/a11y/css_net.drive
```

`tools/a11y/lan_lobby.drive` starts both instances with no session: each walks the menus into the LAN lobby on its own presses, they find each other there, and Start on a takes both into character select. Holding B there takes both back to the lobby; Start again, then closing a alone (`a quit`), returns b to its lobby with the connection lost. It fails on a missing lobby announcement: the opening, the other machine found, starting, and the two returns. It runs with `--lan`, which skips the recording comparison (`docs/a11y/netplay.md` says why). The LAN lobby finds only copies in a LAN lobby, so it runs beside the `css_net.drive` runs unbothered:

```
python tools/a11y/drive_net.py --lan --out-dir build/drive/lan tools/a11y/lan_lobby.drive
```

The LAN run needs a network interface up (Ethernet or Wi-Fi with an address; `lan: no usable network interface` in a log means none) and a Windows Firewall rule letting that build's `melee.exe` take UDP in. A build directory without a rule makes the first run prompt and take focus, so ask the maintainer to allow it first.

A garbled archive (`ARCHIVE <file>: implausible header`, then a PANIC in `lbarchive.c` and exit 3221226505, 0xC0000409, or a hang right after a scene load) was a base port bug, fixed on 2026-10-13 (`file cache: store its own host buffers without the ARAM test`): the file cache copied ARAM instead of a file whenever the heap gave its read buffer an address below 0x01000000, and the cue engine's early audio stream made that likelier (about one run in ten of `main_menu.drive` with `MELEE_A11Y_CUES=1`). It struck on the unlock notices after the title screen (`LbMcGame.usd`, `TyDatai.usd`), at the memory card screen and as a match loads (`SdIntro.dat`, `LbRf.dat`). If it comes back, look in the log for a `[FileCache] STORED: <file> ... [from ARAM]` line for that file before its failed load: a store from a host buffer must never say `[from ARAM]`.

The `lan_lobby.drive` run against a base port build failed once (2026-10-14, the online level side by side) at `wait lobby: entering CSS` after the second Start: b, the base port build, dropped a's first datagram for a session it had not learned yet (`net: dropped a datagram for session ... (ours 00000000`), a disconnected at frame 2, and b hung at `lobby: Connecting...`. Passing runs drop no datagram there. It passed on two unchanged reruns; it is the base port's netcode, not the fork's.

At the base merge level, `run_level.py online --base-b <dir>` (or `--base-b <dir>` on one run by hand) makes instance b a base port build of the merged commit from that build directory: build it in a git worktree outside the repo with its own build directory, with the fork's native Windows build fix applied (`docs/a11y/netplay.md`).

## Out of reach

The port menu and the launcher are out of the driver's reach; they depend on the maintainer play-testing, after which the agent reads the `[a11y]` lines against what they report.

Quick chat logs nothing, so no script checks that steering sends no phrases on online character select (`pc_a11y_chat_buttons`). After a change there, run `css_net.drive`'s D-pad steps with a screenshot of both windows after a second D-pad press: neither may show a chat line ("You: Hello", "Opponent: Hello"). A debug control server (state queries, pause, input injection over localhost) is a possible future direction, not a commitment.
