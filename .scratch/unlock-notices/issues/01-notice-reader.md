# 01 The unlock notice reader

Status: resolved (2026-10-29)
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

### 2026-10-29, implementation (agent)

Implemented as the spec has it: `unlock_speech.hpp/.cpp` (the reader), the hook `pc_a11y_unlock_notice(text_id)` at the end of `un_802FE918` gated by `game_hook_may_speak()`, `a11y_game_prize_string` in `game_access.c` (font slot 2, symbol `SIS_PrizeData`), the scene hook starting each visit, and `GS_PRIZE_INTERFACE` silent in `scene_speech.cpp`.

- **The trophy text is there.** A driven run read string 0x4A at the hook as the whole message: "You got the Kirby Hat 4 trophy!", "…Kirby Hat 5…", "…Marin…". The hook passes 0x4A for unlock number 0x3E and the frame's bound string (`un_803F9D48.x6`) otherwise; the composition (`HSD_SisLib_803A6530`, `803A660C`) indexes the table directly, and `pc_region_sis_index` leaves `SIS_PrizeData` alone, so the two agree.
- **Font glyphs, kept to this table.** `GameTextSource` gained `font_glyphs`, the characters of a screen's own font from 0x4000, a null entry leaving one unknown; only the unlock reader hands one over (`prize_font_glyphs()` in `game_text.cpp`), so every other table decodes as before. A dump of the table (`MELEE_A11Y_TEXT_DUMP=1`, which the hook now does for font slot 2) found three: 0x4000 é four times (Poké Floats' notice, Poké Ball, Pokémon Stadium), 0x4002 once, in the trophy name "Ray Mk {glyph 4002}", the numeral II as in SdMenu, and 0x4001 filling twelve three-glyph strings. Beyond the spec, 0x4002 is mapped to "II", so a Ray Mk II trophy notice is not said "Ray Mk"; 0x4001 stays unknown.
- **A notice with no words** (no string at that number, or a wrong table) says the fork's "Unlock notice." in its place, with the key on a visit's first, logged once, so a press never meets silence. A text the decoder stops in is said up to where it stopped, logged once.
- **Every notice interrupts**, the first too: it follows the title screen's or results' last line, never the proof-of-life announcement.
- **Tests.** `unlock_speech` (new): each notice interrupting, the disc's double space, the key on the first of a visit only and again on the next visit, a trophy notice, the prize font's é in a decoded NTSC-U string, a missing and an undecodable text. `game_text`: 0x4000 and 0x4002 on the prize table, 0x4000 still unknown without it, 0x4001 and past the table unknown. `scene_speech`: the scene silent.
- **Drive script.** `unlock_notices.drive`, in the menus level: the eight texts in order, the key on the first; `run_level.py` counts one "A for next." line and fails on the old placeholder. `main_menu.drive`'s comment says the notices are now said.
- **Docs.** `docs/a11y/native-menus.md`: "Unlock notices and challengers in detail" with the notice half (the scene, the queue, the text and its glyphs, where the fork reads it), the screen inventory pointing to it, `SdPrize` in the game text archive list and the text dump. Issue 03 adds the New challenger screen's subsections there and extends its opening paragraph. The `verify` skill has the script, the `unlock_speech` suite and the challenger fight's notice as out of reach.
- **Verification.** Menus level: build with the "Rollback sections" line, style, unit tests (all but `launcher_data`, known), and `run_level.py menus`, 32 of 32.
- **Left open.** Only the US disc is checked. The results table's "Pokémon KO" bonus (SdRst 285) and SdMenu's Pokémon and Mushroom Kingdom II use the same codes and are still said without the é or the II ("Pokmon KO"); the decoder can now take their glyphs too, a choice for the maintainer. A notice after a challenger fight won is left to the play test (issue 04).
