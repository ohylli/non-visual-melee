# 01 `drive.py --card`: runs with a memory card

Status: resolved (2026-10-29)
Type: task

Spec: `.scratch/memory-card/spec.md`, "Tests". The unlock notices' save tool (`.scratch/unlock-notices/issues/02-challenger-save.md`) builds on this too.

## What

Today `drive.py` always passes `--no-card`, so no agent run reaches a memory card prompt. Add:

- **`--card`**: drop `--no-card`. The card is the run's own: aurora puts it under the run's settings folder (`MELEE_USER_DIR`), `USA/Card A/`, never the maintainer's `%APPDATA%\melee-pc`. An empty folder means a fresh install.
- **`--card SAVE`** (or a separate option, the implementer's call): copy a prepared save file (a `.gci`) into that folder before the run, for a corrupted save or the unlock notices' challenger save.
- **`drive_net.py`** keeps `--no-card`; it does not need the switch.

A throwaway prototype of both is in the investigation's notes (it patched `drive.py` to drop `--no-card` and copied a prepared `USA/` tree into the settings folder). The emulated card's file is `01-GALE-SuperSmashBros0110290334.gci` (`extern/aurora/lib/dolphin/card.cpp`).

Two things to keep in mind:

- The prompt shows only when the run starts at the memory card scene: `--scene ""`, since `MELEE_BOOT_SCENE` skips it.
- **Never combine `--card` with a boot scene that saves**: a boot scene skips loading the card, and the game's first save then writes its default save over the card's file (`.scratch/boot-scene/issues/01-boot-scene-overwrites-save.md`). Refuse `--card` with any `--scene` other than `""`, or warn, whichever is simpler.

## Done when

- `python tools/a11y/drive.py --card --scene "" -c "..."` reaches "Create Game Data?" (seen in the game log or a screenshot until issue 02 speaks it), and a run with a prepared save loads that save.
- `drive.py --help` documents the switch and the boot scene caveat.
- The `verify` skill and `CLAUDE.md`'s "Driving the game" name `--card`.
- `python tools/check_style.py` passes (it covers `src/pc/` only; keep the script's own style).

## Comments

### 2026-10-29, implementation (agent)

Implemented in `tools/a11y/drive.py`; `drive_net.py` keeps `--no-card` (its `args` has no `card`, which `Game` reads with `getattr`).

- **The switch.** One option, `--card [GCI]`: `--card` alone runs with the run's own empty card, `--card GCI` with that save copied onto it first. With the optional argument, `--card script.drive` would take the script as the save; the save is checked at once, and a `.drive` taken that way is refused with a hint (put the script first, or `--card=GCI`). `-c` scripts are not affected.
- **Boot scene.** `--card` with any `--scene` other than `""` is refused (exit 2, before the game starts). `--scene` now defaults to `""` with `--card` and to `title` without, so `drive.py --card -c ...` needs no `--scene ""`.
- **The save's name.** aurora reads only a `.gci` named from its own directory entry, `maker-game-name.gci` (`gci_filename`, `is_gci_filename` in `extern/aurora/lib/card/CardGciFolder.cpp`), and ignores any other. `card_save_place` names the copy that way from the file's entry, and its region folder from the game code's last letter as `get_card_region` does (`USA` for `GALE`), so a save can be called anything; a file whose size is not 0x40 plus 0x2000 per block its entry names is refused. The save is read before the settings folder is emptied, so a run can take the last run's own save straight from `<out>.settings`. The transcript notes `card save: <path>`.
- **Seen in runs.** `--card` reached "Create Game Data?" with Yes hovered (screenshot); A gave "Game Data has been created. …", A again the opening movie, and the save `01-GALE-SuperSmashBros0110290334.gci` (11 blocks) was in `<out>.settings/USA/Card A/`. Passed back with `--card`, the same `--out`, it loaded: the opening movie with no prompt and no press. A renamed copy loaded too. `--scene ""` without `--card` showed "There is no Memory Card in Slot A." The game logs `[ERROR] aurora::card: Failed to close file at idx: 0` when it creates the save; the save is written whole and loads, so it is noise for now.
- **Finding for issue 02: corrupting a fresh save.** Flipping one byte of the first data block is not enough: the game loaded the save with byte 0x100 of sector 1 flipped, of sector 0 (the banner), of sectors 1 and 10 (block 1 and its second copy), of sector 2, 3, 8 or 9 alone, and of 2 and 9. Flipping byte 0x100 of every data sector, 1 to 10 (file offset 0x40 + n × 0x2000 + 0x100), gave "The Game Data on the Memory Card in Slot A is corrupted. Erase this and create new Game Data?" with No hovered (screenshot); sectors 2 to 9 also stopped the load (no opening movie; the prompt not looked at). Why single blocks pass was not looked into.
- **Not added.** No unit test for the naming: the driven runs above are its test, the game being the judge of a name it accepts.
- **Verification.** Quick level: build with the "Rollback sections" line, style, unit tests (all but `launcher_data`, known), and `run_level.py tree_walk launcher_home lan_lobby`, 3 of 3, for the unchanged `--no-card` path through both drivers and the launcher. The runs above, transcripts removed.
