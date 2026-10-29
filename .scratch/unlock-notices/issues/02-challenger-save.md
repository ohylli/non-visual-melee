# 02 The challenger save tool

Status: ready-for-agent
Type: task
Blocked by: `.scratch/memory-card/issues/01-drive-card.md`

Spec: `.scratch/unlock-notices/spec.md`, "The save file" and "The challenger save tool". The working prototype is in `.scratch/unlock-notices/prototype/`: `hsdcard.py` (the block checksum and cipher, a port of `crypt.c`, and the GCI reader and writer), `make_challenger_save.py` (the edit, with the thresholds) and `challenger_quit.drive` (the quick VS match, written for a throwaway wrapper around `drive.py`).

## What

- **`tools/a11y/make_save.py`**: from the prototype. Takes a fresh save and a challenger's name and writes the edited save. Structured so other edits can be added later.
- **The fresh save**: made the first time it is needed by a `drive.py --card --scene ""` run that answers Yes to "Create Game Data?", and kept under `build/` (for instance `build/drive/cache/`, which stays between runs). Never committed: its first sector holds the game's pictures.
- **The maintainer's launch**: one command (a Python script in `tools/a11y/`, or an option of `make_save.py`) that makes a test folder with the challenger save and a copy of the maintainer's `a11y.ini`, and starts `build\melee.exe melee.iso` there with `MELEE_USER_DIR` and `MELEE_CACHE_DIR` set, no `--no-card` and no `MELEE_BOOT_SCENE`. It must never read or write `%APPDATA%\melee-pc` beyond copying `a11y.ini` out of it. This run is the maintainer's own, so it may take focus and play sound; an agent never runs it.
- **The challenger drive script** (`tools/a11y/challenger.drive`): with `drive.py --card <challenger save> --scene ""`, through VS to a match quit from the pause, to the New challenger screen. Until issue 03 it waits on the scene's current "New challenger" line.

## Done when

- `make_save.py` makes a save that brings Jigglypuff, and one that brings Mewtwo, each confirmed by the drive script reaching the screen.
- The maintainer's launch, tried by an agent with its launch step replaced by a dry run (print the command and the environment), makes the folder as described.
- The `verify` skill names the tool and the script, and `CLAUDE.md`'s "Driving the game" the tool and the maintainer's launch.
- `docs/a11y/native-menus.md` has the save file's layout and the challenger's thresholds, in "Unlock notices and challengers in detail".

## Comments
