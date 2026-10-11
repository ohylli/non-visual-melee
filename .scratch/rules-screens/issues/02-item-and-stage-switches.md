# 02 Item Switch and Random Stage

Status: ready-for-agent
Type: task
Blocked by: 01

The slice of `.scratch/rules-screens/spec.md` for the two grid screens, from the main menu tree. Read the spec and issue 01's comments first.

## Machinery

- **Item Switch.** Rows 0 to 30 are items, 31 and 32 the one frequency strip. Item on or off from `MnItemSwData.items` (`mnItemSw_804D6BE8`'s user data) or `confirmed_selection` for the hovered item: `GamePrefs.item_mask` lags for the item just toggled. Frequency from `x21`, 0 None to 5 Very High.
- **Random Stage.** Rows 0 to 28; stage by a fork copy of `mnStageSw_803ED4C4` (static in `mnstagesw.c`; comment naming the original), names from stage select's table. On or off: find where the hovered value can be read, since `stage_mask` lags for the stage last toggled and the screen's data is static (spec, open question 2).
- **Refused toggle.** Find how the reader learns of it (spec, open question 1). A hook at the refused branch is acceptable; it goes in `a11y_hooks.h` like every hook.
- **Item names.** A fork string table by display slot, names as drawn (spec, "Speech: Item Switch").
- **Frequency hint.** Said the first time the strip is reached after each opening of Item Switch.

## Speech in this slice

Wording is in the spec, "Speech: Item Switch" and "Speech: Random Stage". Both screens lose their "No speech yet." notice.

## Done when

- `cmake --build build` passes, and the unit tests pass, with `test_menu_speech` extended: both openings, moving within and between columns, the wrap through the frequency strip, the hint on the first visit only and again after reopening, every frequency word, toggles both ways, the refused toggle, a press that changes nothing.
- The drive script of issue 01, or a second one, opens Item Switch, toggles an item and back, moves to the strip and changes the frequency and back, opens Random Stage, toggles a stage and back, and waits on each expected line. The refused toggle is checked in a run that turns off every stage but one and then restores them, or by the unit test alone if a drive cannot reach it within reason; say which.
- `python tools/check_style.py` passes.
- The `verify` skill updated.
- The primer records whether item names are game text, and how the switch screens' values were read.

## Comments
