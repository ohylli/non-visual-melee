# 07 Character select in single-player modes

Status: resolved (2026-10-01)
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

### 2026-10-01, implementation (agent)

Implemented; hearing it is issue 08.

- **Screenshots** of Classic, Training and Target Test, checked against the rows. Classic draws a row "LEVEL" (the difficulty strip between two arrows) above a row "STOCK" (stock icons between two arrows); Training a second slot with the tab "CP" and its coin on a random portrait, no arrows; Target Test neither. All three have Back alone in the top bar.
- **Two rows, not one.** The issue suggested one row of arrows. They sit at two heights, with the lower-level arrow at x -7.5, the fewer-stocks arrow at 0.75 and both raising arrows near 16, so one row ordered by x would interleave the two settings. The fork builds two rows as drawn: Level (lower, higher) above Stock (fewer, more). Up from the start reaches "Stock: 3, fewer", then the level row, then Jigglypuff in the bottom row.
- **The hook** hands over the two tables that hold the arrows: `pc_a11y_css_frame` gained `misc` (`mnCharSel_803F0EBC`: the difficulty, its arrows, the one name box's joint) and `data2` (the stock count and its arrows). The call in `mncharsel.c` is still one line.
- **Snapshot.** New fields: the match type; the arrows (shown or not, the values, the four rectangles from the game's tables); the port whose pad moves the hand (`local_port`: the hand's own in VS modes, the starting port in single-player modes); and A pressed this frame. In single-player modes the slots after the mode's one (two in Training) read as closed. Before this, a single-player screen read all four slots' name tag windows, and three of those pointers were left freed by the last VS screen. Teams and the handicap rule now count in VS modes only.
- **Words** as drawn: "Level" for the row the issue called difficulty, with the five values from the strip's game text ("Very easy" to "Very hard"), and "Stock". The screen draws "NORMAL" when speech says "Normal". "Level: Normal, lower", "Stock: 3, more"; A says "Easy" or "4". At the end of the range the game does nothing, not even the refused sound; speech says the value again, on the frame the pad shows A on an arrow at that end. The opening is "Classic character select. Player 1, no character.", Training adds "CPU, Bowser.", and a mode started from another controller adds "Steering needs controller 1." Training's CPU is "CPU" ("Holding the CPU's coin", "Bowser, the CPU's coin"). With `--no-card` Classic starts at Very easy with 3 stocks.
- **Steering** runs in single-player modes when controller 1 started the mode, and reads the starting port's pad.
- **Name box.** A free hand on the single-player name box says "Player 1 name tag", from its own joint; unit-tested, not tried in a run.
- **Runs.** `tools/a11y/css_classic.drive` and `tools/a11y/css_training.drive` pass. Classic: Up over both arrow rows to Jigglypuff, A, Ready to Fight, the level up to Easy and back down to Very easy, A again repeating "Very easy", the stocks up to 4, Start, "Stage intro", the match. Training: Pichu for the player, the CPU's coin found on Bowser after six steps and carried to Luigi, Ready to Fight, stage select, the training match. A Target Test run: its opening, Down at the bottom row repeating "Pichu", and Up from the top row to Back ("Back, not while holding a coin").
- **Unit tests.** `css_targets`: the top bar, each arrow and the space between them, the arrows by mode, no arrows while carrying a coin, the name box, `arrow_at_end` for each arrow at both ends, rows in Classic, All-Star and Training, steps over the arrows and their edges, steps from the hand's start, every target at its aim, Training's CPU coin. `css_speech`: openings in Classic, Adventure, Home-Run Contest and Training, the steering note, a name for every single-player mode, the coin and Ready to Fight, a free hand on each arrow, A on an arrow, A at both ends of each range, steps onto arrows and down to them with a coin, Training's CPU coin. `steering`: glides from every target to every other in Classic and Training at delays 0 to 10.
- Not tried in a run: Adventure, All-Star, Event Match, Home-Run Contest and Multi-Man Melee (same code paths as Classic, All-Star without the stock row, the rest as Target Test), and a mode started from another controller, which the drivers cannot do since they feed controller 1.
