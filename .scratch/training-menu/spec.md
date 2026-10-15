# Training menu: speech

Status: agreed 2026-10-15 (design conversation with the maintainer)

The maintainer agreed the scope (the menu only), silence on arriving in Training and on closing, the opening and its hint, row names and values as drawn, no descriptions, no speech for the actions, the rule for held directions, and polling with no new hooks. The wording is a first proposal for the play test to judge.

## Goal

A blind player in Training can open the Training menu and hear where the cursor is, move through its rows and hear each row and its value, and change a value and hear the new one, without the speech falling behind when a direction is held.

How the game's menus run is in `docs/a11y/native-menus.md`. Vocabulary is `CONTEXT.md`: Training menu, row, value, announcement.

## Scope

In:

- **The Training menu**, opened with Start during a Training match: its eight rows, their values, and the cursor.

Out:

- **Training's display during play**: the last hit's damage, the hit count and total damage of the current combo. A later feature; speaking them is match speech, with timing rules of its own.
- **Arriving in Training**: stays silent, as every match does.
- **The actions' results**: A on Item (the item spawned, or not, at the item limit), Reset and Finish say nothing. The game's sounds cover them, and Finish lands on Training character select, which speaks its own opening.
- **Descriptions**: the game draws none, and the fork adds none.

## What the game gives us

From reading `src/melee/gm/gm_1884.c`, `gmvs.c` and `gmtrainingmode.c`, and two driven runs on 2026-10-15. Address-style names are the decomp's.

### State

- The menu's state is the non-static global `gm_80473814` (a `CssSubStruct`, `gm/types.h`, declared in `gm_1884.h`): `x00` the row the cursor is on, `x01` 1 while the menu is open, `menu_values[]` one value per row. The struct holds bitfields, so it is read from a file built with `-mno-ms-bitfields`, as `game_access.c` is.
- The scene's start (`fn_80189B88`, from `gm_Scene_Training_OnEnter`) puts the cursor on Speed and every value at its default. The cursor and the values then persist between openings for the rest of the session; nothing is kept between sessions.
- No fork hook is needed for the menu. `pc_a11y_match_tick` runs after every simulated tick, paused ones included, after the menu's own procs have run, so it sees each tick's finished state. Today it returns early unless the cue system exists.

### Rows and values

The menu is a column on the left; the hovered row is drawn gold with arrows. Labels and values are pictures except the item name.

| Row (`x00`) | Label as drawn | Values (`menu_values[row]`), in Right-press order | Default |
|---|---|---|---|
| 0 | Speed | index into `{2.0, 1.5, 1.0, 2/3, 1/2, 1/4}`, drawn ×2, ×1.5, ×1, 2/3, 1/2, 1/4. Right goes slower | ×1 (index 2) |
| 1 | Item | 0 to 29, an index into `gmTraining_ItemTable` (static), whose entries are an `ItemKind` and an SdTrain string. Not an `ItemKind` itself | Food |
| 2 | No. of CPs | 0 to 2, drawn and applied as 1 to 3 | 1 |
| 3 | CP: | 0 to 5: Stand, Walk, Evade, Jump, Attack, Human | Stand |
| 4 | CP Damage | 0 to 999, drawn as a plain number | 0 |
| 5 | (hidden) | unreachable: the cursor skips it, it is never drawn and nothing reads it | |
| 6 | Camera | 0 to 2: Normal, Zoom, Free | Normal |
| 7 | Reset | none; A only | |
| 8 | Finish | none; A only | |

The items in the menu's order: Food, Maxim Tomato, Heart Container, Ray Gun, Fire Flower, Super Scope, Star Rod, Lip's Stick, Beam Sword, Home-Run Bat, Fan, Hammer, Green Shell, Red Shell, Flipper, Freezie, Mr. Saturn, Poké Ball, Bob-omb, Motion-Sensor Bomb, Super Mushroom, Poison Mushroom, Starman, Parasol, Warp Star, Metal Box, Bunny Hood, Screw Attack, Barrel Cannon, Cloaking Device. That is Item Switch's list without Party Ball, in another order.

### Input

- One controller drives it: the human player's port, from player 0's slot. The D-pad and the stick are the same. Only that player can pause, and only the pauser can unpause; each is locked out for 10 ticks after the other.
- Up and Down move the cursor and wrap (Finish to Speed and back). Left and Right change the value and wrap on every row; Reset and Finish ignore them.
- Every held button auto-repeats: the first press at once, then after 20 ticks one every 8, from 40 ticks every 4, from 100 ticks every 2. CP Damage pauses 25 ticks at 0 and 999 before wrapping, on controller 1 only.
- A on Item spawns the item above the player, with the same sound whether or not it spawned; holding A spawns one after another. A on Reset puts every value back to its default, moves the player to the spawn point and leaves the cursor on Reset. A on Finish ends the match at once, without confirmation. B and the other buttons do nothing; Start closes the menu.
- **Slow motion slows the menu.** The chosen speed stays in force while the menu is open, and a tick the frame timer skips runs no proc, so at 1/4 the menu runs 15 times a second, its repeats four times slower.

### What a change does

- Camera changes apply at once. Speed, the number of CPUs, the CPU's behaviour and its damage apply when the menu closes, every time it closes, so each pause sets the CPUs' percent back to the menu's value.
- While the menu is open the pause word's bit 2 is set and the match cues are silent (`docs/a11y/gameplay-state.md`, "Pause, freezes and slow motion").

### A correction

`native-menus.md` says "a pause notice" is game text on this menu. None shows: `fn_80188EE8` binds SdTrain string 30 to the menu's one text object while paused, then rebinds it to the item name later in the same function, every tick.

## Decisions

### Hosting

- **Polled from the tick hook.** A reader in `src/pc/a11y/` gets a snapshot of the menu (row, open, the hovered row's value) each tick from `pc_a11y_match_tick`, read by `game_access.c`. No new hook in a base port file.
- **The tick hook keeps working with cues off.** `pc_a11y_match_tick` is reorganised so the Training menu reader runs whether or not the cue system exists.
- **The usual speech gate.** Training is never online, but the reader stays behind `game_hook_may_speak()` like every other.
- **Read only.** The reader writes nothing in the game.
- **Only the hovered row is spoken.** A change to any other row's value, such as Reset's, says nothing.

### Speech

- **Opening** (the menu opens): "Training menu." then the hovered row with its value. "Training menu. Speed: x1."
- **The hint**, on the first opening of each Training session (each time the Training scene is entered) only, after the row: "Training menu. Speed: x1. Up and down for rows, left and right to adjust, A on Item, Reset and Finish, Start to close." The cursor remembers its row, so the opening names whichever row that is.
- **Row change**: the row with its value. "Item: Food". Reset and Finish say their names alone.
- **Value change**: the value alone. "x1.5", "Maxim Tomato", "Walk", "57".
- **Wraps**: only the new row or value; nothing says it wrapped.
- **Closing**: nothing.

Row names are as drawn. Where a drawn label's punctuation would read badly next to the value it is tidied: "No. of CPs, 2" and "CP, Stand". If NVDA reads "No." as the word "no" in the play test, the row becomes "Number of CPs".

Value words, as drawn:

| Row | Words |
|---|---|
| Speed | x2, x1.5, x1, 2/3, 1/2, 1/4 (the letter x; fractions left to the screen reader) |
| Item | the names above |
| No. of CPs | 1, 2, 3 |
| CP | Stand, Walk, Evade, Jump, Attack, Human |
| CP Damage | the number |
| Camera | Normal, Zoom, Free |

Item names come from the fork's item names, the same words Item Switch speaks, keyed by the item rather than by either screen's order: one table both screens use, so an item is named the same everywhere. A value the reader does not know is spoken as its number and logged once, as on the other readers.

### Held directions

On every row and for cursor moves alike:

- A change that comes at least a quarter second after the previous one is spoken at once.
- A change that comes sooner is held back; the newest held change is spoken once a quarter second passes with no other. A burst of auto-repeat is heard as its first change, then the value or row it settles on.
- The quarter second is real time, not ticks, so slow motion does not stretch it. It is a constant in the reader, to tune after the play test.
- Closing the menu drops a held announcement; opening it interrupts.

### Tests

- A unit test of the reader (`test_training_speech` or an extension of an existing test) with a fake clock: the opening with and without the hint, the hint once per session and again in the next, every row's name, every value word, wraps, the hidden row never named, Reset leaving other rows' changes unsaid, the held-direction rule (a slow series, a burst, a burst then a pause, a burst then closing), and an unknown value.
- A drive script, `tools/a11y/training_menu.drive`, from the main menu to a Training match: opens the menu, walks every row, changes each value and back, holds Right on CP Damage, closes and reopens on the remembered row without the hint, and waits on each expected line.

### Docs

- `docs/a11y/native-menus.md`: the Training menu's entry corrected (no pause notice) and given what this spec found: the state, the rows and values, persistence, the controller, auto-repeat, apply-on-close, and the menu running at the chosen speed.
- CLAUDE.md: one "Accessibility status" line.
- The `verify` skill: the new drive script and its expected lines.

## Open questions

For the implementer to find out:

1. Which tells the reader the menu is open: `x01`, the pause word's bit 2, or the panel's slide (`anim_frames`). The opening should be said once per Start, and a close and reopen inside the 10-tick lockouts should not double it.
2. Whether input reaches the menu during the slide-in, so that a press on the first ticks is spoken after the opening rather than in place of it.
3. How the reader learns a Training session started, for the hint: the scene hook (`pc_a11y_scene_entered` with `GS_TRAINING`) or the menu's state as `fn_80189B88` leaves it.
4. Where the shared item names live: Item Switch's rows today are keyed by its display slot. A table by `ItemKind` that both readers look up, with a fork copy of `gmTraining_ItemTable`'s kinds (a comment naming the original), is the likely shape.
5. Whether drive steps can hold a direction long enough to test the held-direction rule, or the script presses fast instead.

## Issues

1. `issues/01-reader.md`: the reader, its snapshot and the tick hook change, with unit tests and the primer.
2. `issues/02-drive-script.md`: the drive script, the `verify` skill and the status line. Blocked by 01.
3. `issues/03-play-test.md`: the maintainer, by ear. Blocked by 02.
