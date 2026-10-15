# 03 Play test by ear

Status: resolved (2026-10-15)
Type: task
Blocked by: 02

The maintainer plays the Training menu with NVDA and judges the wording of `.scratch/training-menu/spec.md`.

## What to try

- Open the menu for the first time in a session: the opening and the hint. Close and reopen: no hint, the remembered row.
- Every row and a few values on each. Does NVDA read "No. of CPs" well, or should it become "Number of CPs"? Do the speeds "2/3", "1/2" and "1/4" read well?
- Hold Left or Right on CP Damage and on Item, and hold Down through the rows: is the quarter-second rule quick enough, and is the settled value always heard?
- The slow speeds: open the menu at 1/4 and move about.
- A on Item, Reset and Finish, with only the game's sounds.
- Leave Training and come back: the hint again.

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue, and CLAUDE.md's status line says the Training menu was play-tested.

## Comments

### 2026-10-15, code review (agent)

Fixed in the review: a value changed on a row whose name was never heard now says the row with it. Down then Down then Right quickly used to say "Item: Food" then "2", the held "No. of CPs, 1" replaced by the bare value, so the 2 sounded like Item's; now it says "No. of CPs, 2". Worth listening for in the held-direction tries above.

Left for the play test:

- **A hold is heard as three lines.** The held-direction rule waits a quarter second (`TrainingSpeech::kSettle`), but the game's first auto-repeat comes 20 ticks (a third of a second) after the press, so a hold says the press, the first repeat, then the settled value, where the spec planned two. Options: keep it, or raise the settle above a third of a second so the first repeat is held too. Judge by ear on CP Damage and Item.
- **A press during the first opening waits behind the hint.** A press in the opening's first quarter second is queued after the opening rather than cutting it off, so on a session's first opening, whose hint runs about five seconds, the row reached is heard only after the whole hint; the same press a moment later interrupts. Options: keep it; or queue only after an opening without the hint and interrupt when it carried the hint. Recommendation: the second, if the wait is noticed.
- **Small shared helpers (code, low).** `TrainingSpeech::log_once` and its set repeat `MenuSpeech::log_once`, and `a11y::Clock` is declared in three headers (`launcher_speech.hpp`, `value_file.hpp`, `training_speech.hpp`). A shared log-once helper and one clock header would serve the next reader; recommendation: do it when a third reader would copy them again.

### 2026-10-15, hold time tuned by ear (maintainer)

The quarter second felt a bit slow. The maintainer tried 0.15 s and 0.2 s and chose 0.15 s (`TrainingSpeech::kSettle`). At 0.15 s the game's two repeats after the first come about 0.15 s apart, right on the rule, so a hold says the press, the first repeat, then none, one or both of those two, and the value it settles on; the quicker repeats after them are always held back. The drive script now takes the settled value after the stick is let go, and `run_level.py` accepts seven to nine numbers. The other items above still wait for the play test.

### 2026-10-15, play test by ear (maintainer)

Played with NVDA. The hold time was the one change (above); everything else felt good, so the remaining items are settled as they stand:

- **The wording**, "No. of CPs" and the speeds included, stays as specified. The game draws the slow speeds as bare fractions ("1 / 4", no ×), as the spec has them.
- **A press during the first opening** still waits behind the hint; it was not noticed in play.
- **The small shared helpers** stay as recommended: shared when a third reader would copy them again.
