# 07 Character select in single-player modes

Status: ready-for-agent
Type: task
Blocked by: 05

Speech and steering on the character select of Classic, Adventure, All-Star, Event Match where it has one, the Stadium modes and Training, from `.scratch/character-select/spec.md`. The portraits and the coin work as in VS modes; the layout around them differs. Read the spec first.

## What differs

Facts from the code, to confirm against screenshots of each mode:

- One hand, whichever port started the mode (`mnCharSel_804D6CF0`). The number of hands (`mnCharSel_804D6CF5`) is 1.
- The match type (`CSSMatchType` in the screen's data) tells the modes apart.
- Stock arrows, less and more, in match types below `0xD`: 1 to 5 stocks.
- Difficulty arrows, less and more, in match types up to `0xD`: five levels. The decomp's field names call it a CPU level.
- One name box instead of four.
- Training has two player slots and one hand: the player chooses a character for themselves and for the CPU.
- Ready to Fight shows once the one coin is placed.
- X and Y change the costume while the coin is placed.

## Machinery

- **Whose hand**: the one hand. Steering can only reach port 1; if another port started the mode, speech works and steering stays off. Say so once in the opening announcement ("Steering needs controller 1") rather than ignoring presses silently.
- **Snapshot and rows** grow by the stock and difficulty arrows, from the rectangles in `data2` and `mnCharSel_803F0EBC`, handed over by the frame hook.
- **Words.** The five difficulty levels and the stock count are drawn as pictures or numbers. Take screenshots, and add the levels' names as drawn to the string table.

## Speech and steps in this slice

- The opening names the mode's screen as the scene table does, and the player's state.
- A free hand on an arrow: "Stocks: 3, fewer", "Stocks: 3, more", "Difficulty: Normal, lower", "Difficulty: Normal, higher". The wording is for the play test to judge.
- A on an arrow: the new value alone, "4". At the end of the range the game does nothing; speech repeats the value.
- The arrows are one row of targets between the portraits and the bottom.
- Training: the opening names both player slots, and the CPU's coin is announced as in VS modes.

## Done when

- `cmake --build build` passes, and the unit tests pass, extended to cover the rows of a single-player mode and of Training, each arrow at both ends of its range, and the opening in each.
- Screenshots of Classic, Training and one Stadium mode are checked against the rows the fork builds.
- A drive script under `tools/a11y/` enters Classic, steps to a portrait and chooses, sets the difficulty one lower and the stocks one higher, waits on each announcement, and starts.
- A second script, or the same, does the Training setup: a character for the player and one for the CPU.
- `python tools/check_style.py` passes.
- The primer gains the single-player layout.
- CLAUDE.md: the "Accessibility status" line updated.

## Comments
