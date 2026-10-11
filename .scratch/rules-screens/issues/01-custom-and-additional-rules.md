# 01 Custom Rules and Additional Rules

Status: resolved (2026-10-11)
Type: task

The slice of `.scratch/rules-screens/spec.md` that gives the two list screens their reader, from the main menu tree. Read the spec, ADR-0003, the main menu tree spec's "Leaf screens with a reader" and the primer `docs/a11y/native-menus.md` first.

## Machinery

- **Rows by hovered selection.** The existing readers find rows by centre text; these screens key rows by `hovered_selection`. Extend the reader tables (`menu_names.cpp`) with that row source, keeping the reader pure.
- **Values.** Read through `game_access.c` from `GameRules` (mode, time limit, stock count, handicap, damage ratio, stage selection, stock match time limit, friendly fire, pause, score display, the self-destruct value in `unk_xc`), or the screens' own data where `GameRules` lags. `GameRules` is plain `u8` fields; check it is current at the poll after a press.
- **Descriptions.** Game text by number, as the screens choose them (`mn_802307F8`, `mn_80232D4C`): per value on Rules, Handicap, Stage Selection, Friendly Fire, Pause, Score Display and Self-Destructs, per row elsewhere.
- **Hidden rows.** Follow `mnRulePlus_IsOptionVisible`; the reader never assumes a fixed row list.
- **Return.** A return from a sub-screen to its parent is an arrival without the hint. Tell it apart from a first opening (the previous menu kind was the sub-screen).

## Speech in this slice

Wording is in the spec, "Speech: Custom Rules and Additional Rules": opening with the hint, row changes, rows that open a screen without a hint, value changes with the value's description where it depends on the value, returns without the hint, gaps. Item Switch and Random Stage keep "No speech yet." until issue 02.

## Done when

- `cmake --build build` passes, and the unit tests pass, with `test_menu_speech` extended: both openings, every row's words, a Rules change from Time to Stock and the renamed next row, a value change with and without its description, the time limit at none, 1 minute and 99, the damage ratio at both ends, the self-destruct values, wrap, a hidden row skipped, rows that open a screen, both returns, an unknown row.
- A drive script under `tools/a11y/` opens Custom Rules from the main menu tree, walks every row, changes at least one value per row and returns it, opens Additional Rules and does the same, and waits on each expected `[a11y]` line. `tree_walk.drive` and `leaf_screens.drive` still pass.
- `python tools/check_style.py` passes.
- The `verify` skill lists the new script.
- The primer's rules screen notes are updated where the code turned out different, including the Score Display finding.
- CLAUDE.md: the main menu tree's "Accessibility status" line names the new readers.

## Comments

### 2026-10-11, implementation (agent)

Implemented; hearing it is issue 04.

- **Machinery.** `Reader` in `menu_names.hpp` now has a `RowKey`: `center_text` for Sound, Screen display and Multi-Man Melee as before, `hovered` for the rules screens, which key rows by `hovered_selection`. A `ReaderRow` carries `described_per_value` (a value change says the description after the value), `opens` (the `MenuKind` A on the row opens) and `in_stock_match` (Custom Rules' Time Limit row becomes Stock). The reader stays pure: `MenuSpeech` gets the rules and the hovered row's description in the snapshot. Issue 02's grids can key rows by `hovered_selection` the same way, with new `RowValue` kinds; for issue 03 the reader needs only a snapshot filled in `GS_CSS`.
- **Values** come from `GameRules` (`A11yRules` in the snapshot, `rules_state` in `game_access.c`). Both screens' own update copies every value there on the frame it changes (`fn_802309F0`, `fn_80232F44`), the same frame the input function changed `confirmed_selection`, and the menu frame hook runs before the objects update, so the next poll sees the press. The drive run confirms it: each line follows its press by one press length. The self-destruct value is kept as stored (0 is -1, 1 is 0, 2 is -2) and turned into words in `menu_names.cpp`. `Mode` and `StageSelectMode` are mirrored in `menu_kinds.h` and checked at compile time like the menu kinds.
- **Descriptions** are game text by number, chosen in `game_access.c` from the screens' own tables as `mn_802307F8` and `mnRulePlus_GetDescIdx` choose them (`mn_803EC818` with `mn_StockCountTextId`, `mn_803ED2E8`), for the value in `GameRules`. The tables are declared in no header, so `game_access.c` declares them as the decomp files define them.
- **Hidden rows.** The reader keys by the hovered row and keeps no row list, so the rows `mnRulePlus_IsOptionVisible` hides are never met; a test covers Pause to Self-destructs with Score Display hidden.
- **Returns.** An arrival on a screen one of whose rows opens the screen the reader saw last is a return: title, row and description, no hint. After `forget()` (a scene entered) it is an opening with the hint.
- **Words.** As the spec has them. The opening on a row that opens a screen keeps the key hint ("Custom Rules. Item Switch. Left and right to change. ..."); that happens only after `forget()`, since the screens open on their first row.
- **Score Display (spec open question 5), answered.** Its visibility bit, `x186C & 1`, is rewritten from the recorded unlocks by every unlock check (`gm_80172898` through `fn_8017280C`), including the one on each Start on the title screen. Unlock 0x17 is recorded only by the check after a VS match, once the save's VS KO count reaches 5000 (`fn_80173644`); Random Stage's 0x18 by any check once the main stages are unlocked, as in every `--no-card` save. So agent runs show Random Stage and never Score Display, and a forced `x186C` before the title screen is undone by its Start. A run with the test forced true (a temporary local patch, not committed) showed the row drawn between Pause and Self-Destructs, and the reader said "Score display: Off. Score will not be displayed on screen." and "On. Score will be displayed on screen." The earlier online run with `x186C` pinned to 0xFF was not repeated, so why it showed no row is not settled; the pin happens at the session handshake, and nothing found here would undo it before character select's rules screen. Issue 03 can look when it drives the screens online. The primer has the finding under "Score Display".
- **Tests.** `test_menu_speech` covers both openings, every row of both screens, Time to Stock with the renamed row and back to a time mode, value changes with and without descriptions, the time limit at none, 1 minute and 99, the damage ratio at 0.5 and 2.0, the three self-destruct values, wrap both ways, a hidden row skipped, Left and Right on a row that opens a screen, the three returns, a return after `forget()`, and an unknown row and value. `tools/a11y/rules_screens.drive` walks both screens from the main menu tree.
- **Verification.** Build, `check_style.py`, unit tests (only `launcher_data` fails, the known Windows failure), a bounded `title` run, and the Menus level: all fourteen menu drive scripts pass. The first `rules_screens.drive` run lost every press to the input driver at the unlock notices and passed on an unchanged rerun.
- **For issue 02**: Item Switch and Random Stage still say "No speech yet."; the return wording from them is in place. The default stock match time limit in a `--no-card` save is none.

