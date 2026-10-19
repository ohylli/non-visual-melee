# 01 A settings folder of each run's own

Status: ready-for-agent
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
