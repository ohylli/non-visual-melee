# 01 `drive.py --card`: runs with a memory card

Status: ready-for-agent
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
