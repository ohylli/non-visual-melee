# 02 The memory card prompt reader

Status: ready-for-agent
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
