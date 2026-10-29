# 01 The unlock notice reader

Status: ready-for-agent
Type: task

Spec: `.scratch/unlock-notices/spec.md`. Read it first, then `docs/a11y/native-menus.md` (game text) and `src/pc/a11y/results_speech.cpp` (the nearest reader of game text by number).

## Machinery

- **Hook**: `pc_a11y_unlock_notice(text_id)` at the end of `un_802FE918` (`ifprize.c`), declared in `a11y_hooks.h`, gated in `hooks.cpp` by `game_hook_may_speak()`.
- **The words**: an accessor in `game_access.c` beside `a11y_game_results_string` reads `SIS_PrizeData` on font slot 2 by number, with its own symbol check. Confirm with a run that the trophy notices' string 0x4A holds the composed text when the hook fires.
- **Glyph 0x4000** decodes as "é" for this table (`game_text.cpp`), with a test in `test_game_text`. Check how 0x4000 and up are resolved today ("a glyph from the screen's own font file") and keep the mapping specific to where it is known to hold.
- **The reader** (`unlock_speech.hpp/.cpp`, new): says each notice, interrupting, with "A for next." on the first of a visit; the scene hook starts a visit.
- **Scene words**: `GS_PRIZE_INTERFACE` becomes silent in `scene_speech.cpp`.

## Done when

- A driven run to the main menu (`tools/a11y/main_menu.drive`) says the 8 notices in the spec's order, the first with "A for next." and the others without, and no "Unlock notice. No speech yet."
- A drive script checks those lines, in the `verify` skill's menus level; `main_menu.drive`'s comment mentions the notices are now said.
- The unit tests the spec lists for notices pass.
- `cmake --build build` passes with the "Rollback sections" line, as do the unit tests and `python tools/check_style.py`.
- `docs/a11y/native-menus.md` has the notice half of "Unlock notices and challengers in detail" (spec, "Docs").

## Comments
