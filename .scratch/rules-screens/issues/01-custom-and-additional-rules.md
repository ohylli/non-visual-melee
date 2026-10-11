# 01 Custom Rules and Additional Rules

Status: ready-for-agent
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
