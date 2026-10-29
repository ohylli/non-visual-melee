# 02 The challenger save tool

Status: resolved (2026-10-29)
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

### 2026-10-29, implementation (agent)

Implemented as the issue has it: `tools/a11y/make_save.py` (the prototype's cipher and edit in one file, with a `show` command), `tools/a11y/challenger.drive`, the maintainer's launch as `make_save.py ... --play`, and a `make_save` ctest suite. The prototype is removed; the tool replaces it.

- **The tool.** `make_save.py challenger NAME [--out GCI]` writes `build/drive/challenger.gci` by default; NAME is any of the 11, case, spaces, punctuation, "and" and a leading "Mr" ignored (`jigglypuff`, `dr. mario`, `gameandwatch`). An edit is a function over the `GmSaveData` bytes, applied to every copy of block 1 (`edit_save`), so another edit (an unlock left pending) is one more function and subcommand. `show GCI` prints the locked characters, the VS match count and play time of each copy.
- **The fresh save** is `build/drive/cache/fresh_save.gci`, made the first time by `drive.py --card= memcard_create.drive` in the `--cache-slot` given (default 1), copied out through a temporary name, its transcript and settings folder then removed (kept on a failure). Delete it to have it remade, after a base merge that changes the save.
- **A save the cipher no longer fits** is refused: a block 1 copy is taken only where its checksum holds once decrypted, and a file with none is an error rather than a save the game would call corrupted. A re-encryption check against the game's bytes was dropped as redundant: the decryption's checksum is the check against the game, and the round trip is the unit test's.
- **Seen in runs.** Jigglypuff's save: the script reached `"New challenger"` in about 23 seconds, the stats panel on the way (`"Time Battle. No contest. ..."`, `"KOs and falls. ..."`). Mewtwo's save (`make_save.py challenger mewtwo --out ...`): the same, and a screenshot showed Mewtwo's silhouette under "A new foe has appeared!". The save's count stays at the threshold after the quit match (`gm_WasMatchCanceled` skips counting), as the spec has it.
- **The maintainer's launch.** `make_save.py challenger NAME --play` empties `build/play/USA/Card A`, writes the save there, copies `%APPDATA%\melee-pc\a11y.ini` to `build/play/a11y.ini` (or removes a stale copy if there is none), and starts `build\melee.exe <repo>\melee.iso` with `build\` as its working directory (the gamepad mappings are read from `resources/` relative to it), `MELEE_USER_DIR` and `MELEE_CACHE_DIR` set to `build/play`, `MELEE_LOG_FILE` to `build/play/melee-pc.log`, `MELEE_BOOT_SCENE` and `MELEE_A11Y_SETTINGS` removed, no `--no-card`; its console output goes to `build/play/console.log`, and its own process group keeps Ctrl+C in the terminal from closing it. `--dry-run`, run by the agent, made the folder (the save, and `a11y.ini` byte for byte the maintainer's) and printed the command, the working directory and the environment changes; the game was not started. The folder was removed afterwards.
- **run_level.** `challenger` joins the menus level: `make_save.py challenger jigglypuff --out build/drive/level/challenger.gci` runs first in the run's cache slot, then the script with `--card=` that save. No `ABSENT` or `COUNT` entry yet: the script's last wait is the check.
- **Docs.** `docs/a11y/native-menus.md`: "The save file" and "Challengers and their thresholds" after the notice subsections, and the section's opening paragraph names them. The `verify` skill: the `make_save` suite, the tool and `--play --dry-run` under "Every game run", and `challenger.drive` among the menu scripts. `CLAUDE.md`'s "Driving the game": the tool, the fresh save, the script and the maintainer's launch.
- **Verification.** Build with the "Rollback sections" line, style, unit tests (all but `launcher_data`, known; `make_save` passes), and `run_level.py menus`, 33 of 33 after `training_menu` failed once on its number count (10, not 7 to 9) and passed on an unchanged rerun.
- **For issue 03.** The script's last line, `wait '"New challenger"$'`, is the one to change; for Mewtwo, `python tools/a11y/make_save.py challenger mewtwo --out build/drive/mewtwo.gci`, then `python tools/a11y/drive.py tools/a11y/challenger.drive --card=build/drive/mewtwo.gci` (remove the save and transcript after). The run has one human, port 1, with a CPU on port 2; `drive.py` drives port 1 only, so a two-human run needs another way in, or the unit test alone.
- **Left open.** The maintainer's test folder has only their `a11y.ini`: their controller and keyboard bindings (`keyboard_bindings.dat`, the `.controller` files in `%APPDATA%\melee-pc`) are not copied, as the issue restricts, so the game starts on the default bindings; copying them too is the maintainer's call. Its shader caches start empty in `build/play`, kept between launches.
