# 01 A boot scene with a memory card overwrites the save

Status: needs-triage
Type: task

Found while looking for a way to reach the New challenger screen in a test (2026-10-29). Not an accessibility bug: it harms any player or developer who sets `MELEE_BOOT_SCENE` while a memory card is present. Today every driver passes `--no-card`, which is why no agent run has met it.

## What happens

`MELEE_BOOT_SCENE` (`src/melee/gm/gmboot.c`, `pc_boot_scene`; started by `gm_801A4510` in `gm_1A3F.c`) starts the game in the chosen mode instead of `GM_BOOT`, so the memory card scene never runs and the save is never loaded. The card status stays at OK from `lbCardGame_Init`, so the first save the game makes (`lbCardGame_SetupArchive`) writes the default save data over the card's file. That happens at VS results (`gmVsMelee_ExitResults`) and at the New challenger state's entry (`gm_ModeState_Approach_OnEnter`), and likely at every other autosave point.

Confirmed in a run: a card seeded with 49 VS matches, `--scene title`, one VS match; afterwards the card held the default save plus 1 match, with no unlocks.

## Directions to weigh

- Load the save (or run the memory card scene's load step without its prompts) before entering the boot scene's mode.
- Or mark the card as not to be written (the status `--no-card` ends in) whenever a boot scene is set, so a boot scene never saves.
- Or refuse a boot scene unless `--no-card` is given.

A fix that helps every player stays unmarked in its own commit on `master` (CLAUDE.md, the isolation rule). Reporting it to the base port happens only if the maintainer asks.

## Comments
