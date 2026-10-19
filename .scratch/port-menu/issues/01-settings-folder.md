# 01 A settings folder of each run's own

Status: resolved (2026-10-19)
Type: task

The first piece of the tooling in `.scratch/port-menu/spec.md` ("The tooling", "Reaching it from a drive script"). Once drive scripts can press keys in the port menu, a run that changes a value would write it to the maintainer's own settings. This issue moves every driven run into a settings folder of its own first.

## Machinery

- **`MELEE_USER_DIR`, a base port fix in its own unmarked commit.**
  - When set, it replaces the settings folder wherever the port takes it from `SDL_GetPrefPath(NULL, "melee-pc")`: `main.c`'s `userPath` (which holds `launcher.cfg`, the controller bindings and the user's `gamecontrollerdb.txt`), and as many of the other users as the spec's open question 5 settles.
  - The cache folder keeps its own switch (`MELEE_CACHE_DIR`).
  - Not marked `a11y`: it helps every tester.
- **The fork follows it.** `cue_settings_path` in `cue_system.cpp` uses the same folder when `MELEE_A11Y_CUE_SETTINGS` is unset.
- **`drive.py`** gives each run a fresh folder under `build/drive`, beside its transcript and removed before the game starts, as the run's cue settings file is today.
- **`drive_net.py`** gives each instance its own folder. Instance b from `--base-b` is a base port build without the switch, which keeps reading the real folder. Say so where `--base-b` is documented.

## Done when

- A driven run writes nothing under the maintainer's AppData `melee-pc` folder. Check its modification times, or a file a run would create, such as `launcher.cfg` after a fresh start.
- The `verify` skill's menus and online levels pass, every difference explained. In particular, find out whether any script relied on the maintainer's settings (spec, open question 5) and fix the script, not the settings.
- `python tools/check_style.py` passes.
- The `verify` skill and CLAUDE.md's "Driving the game" say that driven runs start from default settings in their own folder, and that `MELEE_USER_DIR` names it. `docs/a11y/netplay.md` says the same where it covers the base port build.

## Comments

### 2026-10-19, implementation (agent)

Implemented: every driven run starts from default settings in a folder of its own, and none writes the maintainer's `%APPDATA%\melee-pc`.

- **The switch (base port commit, unmarked).** `pc_user_dir` in `src/pc/main.c`, declared in `pc.h`: `SDL_GetPrefPath(NULL, "melee-pc")`, or the folder `MELEE_USER_DIR` names, with a separator added and the folder made if missing. It returns what `SDL_GetPrefPath` returns (freed with `SDL_free`), so each user changed one call. README's environment table has a row for it. `tools/test_net_match.c` stubbed `SDL_GetPrefPath` for `net_match.c` and now stubs `pc_user_dir`; that test is Linux-only and was not run here.
- **Open question 5, which users: all of them, through one helper.** `main.c`'s `userPath`, and with it everything aurora keeps there (`launcher.cfg`, the keyboard bindings, `controller_ports.dat`, the user's `gamecontrollerdb.txt`, `imgui.ini`, the memory card folder `USA`); the net identity (`net_match.c`); the DHT node cache (`net_dht.c`); and two the spec did not list, user textures (`textures.cpp`, which also makes its `textures` folder) and music (`music_stream.cpp`), since a texture pack or music in the maintainer's folder would change what a run shows and plays. The shader caches keep `MELEE_CACHE_DIR` and stay in the per-user folder when only `MELEE_USER_DIR` is set.
- **The fork follows it.** `cue_settings_path` (`cue_system.cpp`) takes `pc_user_dir`. The drivers no longer set `MELEE_A11Y_CUE_SETTINGS`: a run's cue settings are the `a11y-cues.ini` its fresh folder gets. A non-empty one in their environment still passes through; an empty one is dropped, since it would mean no file at all.
- **The folder, for issue 03.** `drive.settings_dir(out)` is `<out>.settings`, beside the transcript (`build/drive/last.log.settings`, `build/drive/net/a.log.settings`); `drive.fresh_settings_dir(out)` empties and makes it, and `Game.__init__` calls it just before starting the game, keeps the path as `Game.settings_dir` and passes it as `MELEE_USER_DIR`. A `launcher.cfg` for the launcher mode goes in between those two steps. `drive_net.py` needs nothing of its own: each instance's `Game` gets a folder.
- **Open question 5, did a script rely on the maintainer's settings: no.** With cues on, the 17 menu scripts, the 6 match scripts, the 4 online runs and the 4 online runs with `--base-b ../melee-pc-base/build` all passed on the first try. Their spoken lines against transcripts from before the change differ only in random rolls (the CPU's random character, the Random stage, where the Training CPU's coin starts), which also differ between two runs of the new code. Why nothing moved:
  - Unlock everything is off by default, but the `--no-card` save already unlocks every character and stage, so no locked tile or portrait appears.
  - The player name is not heard: the LAN lobby lists machines by host name, and the net identity is made only by internet matchmaking, which no script reaches. Online, `net: unlock forced 07ff07ffff010101 (was 07ff07ff00000000)` is the same as before.
  - The stage select's Random roll was already random from run to run.
- **What does change, for whoever reads a driven run.** The maintainer's `launcher.cfg` had fullscreen on and the master volume turned down; driven runs now open in a window at the base port's size, and the cue engine mixes at the default master volume (`pc_a11y_master_volume`), so a cue dump from a driven run reads louder than one recorded before this change. aurora no longer logs `Loaded GC Card Image` (the run's card folder is empty; `--no-card` keeps the card absent either way).
- **Checked.** The modification times of everything under `%APPDATA%\melee-pc` were the same after the menus, match and online runs as before them; each run's folder held `launcher.cfg`, `imgui.ini`, `a11y-cues.ini`, `textures` and `USA`. The `--base-b` runs then rewrote `launcher.cfg` and `imgui.ini` there, as the issue expects of a base port build. Build, `python tools/check_style.py`, unit tests (`launcher_data` failing as known).
- **Docs.** CLAUDE.md's "Driving the game", the `verify` skill ("Every game run", the bounded run's command, the cue settings bullet, `tools/smoke_test.py`, the base merge level), `docs/a11y/netplay.md` (the base port build), `docs/a11y/cues.md` (where the file is), and the `base-merge` skill: a new `SDL_GetPrefPath` call in a drop must go through `pc_user_dir`, or driven runs reach the maintainer's folder again.
- **Left open.** The base port worktree's build lacks the switch, so an online run with `--base-b` still writes the maintainer's `launcher.cfg` and `imgui.ini` (contents unchanged). Carrying the fix there uncommitted, like the CMake fix, would stop it, at the cost of b no longer being the base port as released. Offering the switch upstream is the maintainer's call. `MELEE_USER_DIR` is read with `getenv`, as `MELEE_CACHE_DIR` and `MELEE_LOG_FILE` are, so on Windows a path outside the ANSI code page would not work; the drivers' paths are plain ASCII.

### 2026-10-19, review (agent)

Review of issues 01 to 03 (`f79f649..f7c4345`). Fixed: `pc_user_dir` logs a `MELEE_USER_DIR` it cannot make (`MELEE_USER_DIR: cannot make <path>: <why>`); `tools/test_custom_music_stream.cpp` (built by no target) stubs `pc_user_dir`; the `verify` skill's bounded run and `smoke_test.py` commands add `MELEE_CACHE_DIR`, since `MELEE_USER_DIR` leaves the shader caches in `%APPDATA%\melee-pc`.

For the maintainer:

- **A `MELEE_USER_DIR` that cannot be made still falls back.** aurora then takes `SDL_GetPrefPath` (the player's folder, or one named after `MELEE_WINDOW_TITLE`) and the launcher `.`, while the net identity, textures and music go without; the review's run with a bad path rewrote `imgui.ini` in `%APPDATA%\melee-pc` (same size, window state only) and left a `launcher.cfg` in the working directory. Options: exit with the error when the switch is set but unusable, or keep the logged fallback. Recommendation: exit, since whoever sets the switch asked for isolation; it touches only runs that set it.
- **`CONTEXT.md` lacks "settings folder"** (and "event pipe", "key pipe"), now used across CLAUDE.md, the skills, the primers and the code. Worth adding beside **Setting** if they are to stay domain words.
