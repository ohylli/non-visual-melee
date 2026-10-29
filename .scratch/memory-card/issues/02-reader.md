# 02 The memory card prompt reader

Status: resolved (2026-10-29)
Type: task
Blocked by: 01

Spec: `.scratch/memory-card/spec.md`. Read it first, then `docs/a11y/native-menus.md` (game text) and `src/pc/a11y/results_speech.cpp` (the nearest reader of game text by number).

## Machinery

- **Hooks**: `pc_a11y_memcard_prompt(kind, text_id)` at the start of `gm_801AEE6C` and `pc_a11y_memcard_frame(hovered_choice)` at the top of `gm_Scene_MemCard_OnFrame` (`gmscmemcard.c`), declared in `a11y_hooks.h`, gated in `hooks.cpp` by `game_hook_may_speak()`.
- **The words**: an accessor in `game_access.c` beside `a11y_game_results_string` reads `SIS_MessageData` on font slot 3 by number, with its own symbol check. Extend `MELEE_A11Y_TEXT_DUMP` to the table if that is cheap, to confirm the ids.
- **The reader** (`memcard_speech.hpp/.cpp`, new): the prompt last shown, held for a whole frame before it is said; the choice last said; whether a prompt with a choice was already said this run (the keys) and whether any prompt was (queue or interrupt). The fork's own words ("Left and Right choose, A confirms.", "A to continue.") live in the reader or a small names file, as the other readers keep theirs.
- **Scene words**: `GS_MEMCARD` stays silent in `scene_speech.cpp`.

## Done when

- The prompts are said as the spec's "What is said" has it, and the unit tests the spec lists pass (`test_memcard_speech`, new).
- In driven runs (`--card --scene ""`), the speech log holds:
  - the fresh install's prompt with "Yes" and the keys, "No" after Right, "Continue without saving or loading Game Data? OK.", "Try Again", the Create prompt again, and, on another run, Yes and "Game Data has been created. … A to continue." with no "Creating new Game Data" anywhere;
  - a corrupted save's prompt with "No" hovered (prepared by flipping a byte of the first data block of a fresh save, as the investigation did);
  - with `--no-card --scene ""`, "There is no Memory Card in Slot A. A to continue."
- Those runs become drive scripts in `tools/a11y/` and join the `verify` skill's menus level; the corrupted save is made at run time from a fresh one, never committed (its first block holds the game's banner and icons).
- `cmake --build build` passes with the "Rollback sections" line, as do the unit tests and `python tools/check_style.py`.
- `docs/a11y/native-menus.md` has "Memory card prompts in detail" (spec, "Docs"), and `CLAUDE.md` an "Accessibility status" line naming the play test as waiting (issue 03).

## Comments

### 2026-10-29, implementation (agent)

Implemented as the spec has it: `memcard_speech.hpp/.cpp` (the reader, with the fork's own words), the two hooks in `gmscmemcard.c` gated by `game_hook_may_speak()`, `a11y_game_memcard_string` in `game_access.c` (font slot 3, symbol `SIS_MessageData`), and the scene hook starting each visit. `GS_MEMCARD` was already silent in `scene_speech.cpp`.

- **"Held for a whole frame" is two frame starts.** The frame hook runs at the top of `gm_Scene_MemCard_OnFrame`, before the frame's logic shows the next prompt, so at the first frame start after a prompt was shown the frame that may replace it has not run yet. The reader says a prompt at the second frame start after it, with the choice hovered then: the moved default, or a move made in between. "Creating new Game Data" and "Erasing the corrupted data" never reached the speech log in any run.
- **Each visit starts afresh** from `pc_a11y_scene_entered`: the scene zeroes its choice on entry, and without the reset a later `GM_MEMCARD` visit would compare its first frames with the last visit's choice. The keys and the queueing stay per game run.
- **Choice names** come from SdMsgBox 2 to 5; should the table lack one, the fork's word stands in, logged once, so a move is never silent.
- **The text dump** covers the table: `menu_text_dump_once` takes a font slot, and the memory card frame hook dumps slot 3 under `MELEE_A11Y_TEXT_DUMP=1`. 25 strings on the US disc, none malformed, no font glyphs; the spec's table matches them.
- **Correction to the spec and this issue: the corrupted save.** Flipping a byte of the first data block does not corrupt a save; the game loads it (issue 01's comments). Byte 0x100 of every data block but the banner, blocks 1 to 10, does. `drive.py --corrupt`, with `--card GCI`, does that to the copy it puts on the card; `run_level.py` runs `memcard_create.drive` first and hands `memcard_corrupted.drive` the save it made, so no save is committed. Recorded in `docs/a11y/native-menus.md`, "The card on PC".
- **Drive scripts**: `memcard_create.drive` (Yes), `memcard_no.drive` (No, Try Again, the Create prompt again, OK, with two "No" lines counted so a press at the end is checked silent), `memcard_no_card.drive` (`--scene ""`), `memcard_corrupted.drive`; the last three in the menus level, `memcard_create` inside `memcard_corrupted`'s run. `run_level.py` fails `memcard_create` and `memcard_corrupted` on a line saying either one-frame prompt. Note for scripts that need a card in `run_level.py`: `ARGS` goes before the script, so an empty card is `--card=`; a bare `--card` takes the script as its save.
- **Verification.** Menus level: build with the "Rollback sections" line, style, unit tests (all but `launcher_data`, known; `memcard_speech` new), and `run_level.py menus`, 31 of 31.
- **Left open.** Only the US disc is checked. The prompts after a failed autosave (`GM_MEMCARD`: Overwrite, Continue without saving?) were not reached in a run; they go through the same two hooks and the same reader. The play test is issue 03.
