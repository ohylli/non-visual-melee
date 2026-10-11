# 02 Item Switch and Random Stage

Status: resolved (2026-10-11)
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

### 2026-10-11, implementation (agent)

Implemented; hearing it is issue 04.

- **Machinery.** Both screens are readers keyed by `hovered_selection` (`menu_names.cpp`), with two new `RowValue` kinds: `switched` (an item or stage on or off) and `item_frequency`. A `Reader` now says whether the screen draws a description line (`described`; these two do not, so nothing is missed or logged) and what a refused change says (`refused`). A `ReaderRow` can carry a hint of its own, said the first time the row is reached after the screen opens (the frequency strip's two rows, 31 and 32, share it), and a stage, named by stage select's table (`spoken_name()`). Random Stage's rows are one fork table from row to stage: a copy of `mnStageSw_803ED4C4` carried through `gm_801641CC`'s table (`lbl_803B7808`), with a comment naming both. Screenshots of both screens match the item and stage tables row for row.
- **Values (spec open question 2), answered.** `confirmed_selection` holds the hovered row's value on both screens: the input function sets it from the screen's own data on every move, flips it on A and steps the frequency on the strip, and the screen's object copies it back into its data the same frame. So the snapshot carries it as `A11yMenuState.confirmed`, and the reader needs neither the masks, which lag, nor Random Stage's static data.
- **Refused toggle (spec open question 1), answered.** Nothing in the screen's state records it: the refused branch of `fn_80235F80` plays the sound and changes nothing. A hook there, `pc_a11y_stage_switch_refused`, sets a flag in `hooks.cpp`; the next menu frame takes it into `A11yMenuState.refused`, taking it even while rollback silences speech, so a refusal in a re-run frame is said once, by the first frame read after it. That is one line in `mnstagesw.c` (which gets `a11y_hooks.h` through `inlines.h`).
- **Words.** As the spec has them. Item names are spoken from the fork's table though they are game text (below).
- **Item names are game text.** The text dump shows each column as one SdMenu string (2 and 3; 4 and 5 are the same columns, 5 saying "Proximity Mine" for the Motion-Sensor Bomb). Random Stage's names are game text too, one string per stage. The fork keeps its tables: the "é" of "Poké" is a glyph the decoder does not know, and Random Stage draws "Temple" where stage select's table says "Hyrule Temple"; the reader says "Hyrule Temple". The primer has the finding under "The switch screens".
- **Tests.** `test_menu_speech` covers both openings, every item's and every stage's name, moving within and between columns and the clamp from Flipper to Party Ball, the wrap through the strip from both columns, the hint on the first visit only and again after reopening and after `forget()`, every frequency word with the wrap, toggles both ways, the refused toggle, presses that change nothing (and a stray refusal on Item Switch), and an unknown frequency. `tools/a11y/rules_switches.drive` walks both screens from the main menu tree; the refused toggle is checked there, in a run that turns every stage off but Rainbow Cruise, presses A on it, and turns them all back on (about a minute and a half). `rules_screens.drive` waits on the new openings instead of "No speech yet."
- **Verification.** Build, `check_style.py`, unit tests (only `launcher_data` fails, the known Windows failure), a bounded `title` run, and the Menus level: all fifteen menu drive scripts pass first time, no press lost.
- **For issue 03.** The switch screens need nothing from the SdSlChr table: their names are fork tables and they have no description line. `pc_a11y_css_frame` feeding the menu reader must take the refused flag the way `pc_a11y_menu_frame` does, before the speech gate, or a refusal on character select's Random Stage goes unsaid (the flag then waits for the next main menu frame, an arrival, which ignores it). The frequency hint resets with `forget()`, which the spec has on each opening from character select.
