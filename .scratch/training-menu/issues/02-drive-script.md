# 02 The drive script

Status: resolved (2026-10-15)
Type: task
Blocked by: 01

The drive script of `.scratch/training-menu/spec.md` ("Tests"), and the records that come with a landed feature. Read the spec and issue 01's comments first. `tools/a11y/match_position.drive` already reaches a Training match and opens the menu.

## Script

`tools/a11y/training_menu.drive`, from `main_menu.drive` into a Training match:

- Open the menu: the opening with the hint.
- Walk every row down to Finish and on round to Speed, the hidden row never named.
- On each value row, change the value once each way; on Speed, the six words.
- Hold Right on CP Damage (or press fast, spec open question 5): the first value, then the settled one, nothing between.
- Close on CP Damage, reopen: the opening on CP Damage, without the hint.
- Finish: Training character select's opening.

## Done when

- The script passes, waiting on each expected line.
- The `verify` skill lists the script and its expected lines at the Menus level.
- CLAUDE.md gets its "Accessibility status" line for the Training menu.
- `build/drive` left clear of the script's transcripts.

## Comments

### 2026-10-15, implementation (agent)

`tools/a11y/training_menu.drive` is in, about 30 seconds, and passes waiting on each expected line, with cues off and with `MELEE_A11Y_CUES=1`.

- **The route.** From the title screen through 1-P Mode to Training, Pichu chosen by D-pad as in `css_training.drive`, then Start on stage select for a random stage; `press Start until` the opening with the keys, every second, so the script does not need to know when the match lets it pause. Picking a fixed stage would add a search and a glide for nothing the menu cares about.
- **The steps**, each with its wait and a 0.3 s sleep after it, so every press comes past the quarter second and is said at once: Speed's six words by Right (`"2/3"`, `"1/2"`, `"1/4"`, `"x2"`, `"x1.5"`, `"x1"`, the wrap included); Down over every row to Finish and round to Speed, each value row's value changed once each way (`"Maxim Tomato"` and `"Food"`, `"2"` and `"1"`, `"Walk"` and `"Stand"`, `"1"` and `"0"`, `"Zoom"` and `"Normal"`); Up round from Speed to CP Damage, so the hidden row is skipped both ways; Right held 2 s on CP Damage; Start to close, Start to reopen on `"Training menu. CP Damage: <settled>."` with the `$` anchor shutting out the keys; Down to Finish and A, waiting for Training character select's opening.
- **Spec open question 5: a drive step can hold a direction.** `press Right 2000` holds it for the two seconds and the game auto-repeats through it, so the script holds rather than presses fast. The spec's "the first value, then the settled one, nothing between" became three lines: the game's first repeat comes 20 ticks (about 0.35 s) after the press, past the quarter second, so it is said too. Measured three times: `"1"`, `"2"` 0.35 s later, then `"21"` a quarter second after the last repeat, each time (issue 01's probe settled on `"20"`). The script keeps the settled number in a named group (any number from 3 up) and expects it in the reopening, rather than pinning it. `native-menus.md`'s auto-repeat entry now says so.
- **Absent lines.** A script cannot fail on a line too many, so `run_level.py` checks two things after `training_menu`: no `[a11y] training menu row` line (the hidden row, or a value the reader cannot name), and exactly seven speech lines that are a number alone (No. of CPs and CP Damage changed and put back, then the hold's three), so a repeat said between the hold's lines fails the run. The count needs no change when the settled number varies.
- **With cues on**, both position cues start as the match begins, stop on the tick of each opening just after its line, and start again at the close between; the `verify` skill lists that with the other scripts' cue lines.
- **Records.** `run_level.py menus` runs seventeen scripts now; the `verify` skill lists the script and its expected lines at the Menus level; CLAUDE.md has the Training menu's "Accessibility status" line. `build/drive` holds only the default transcripts.
- **Verification**, menus level: build with the "Rollback sections" line; style; unit tests, all 26 passing (`launcher_data` failed on the first run and passed on the second, as known); a bounded `title` run; the seventeen menu scripts in one `run_level.py menus` call, all passing first time; `training_menu.drive` alone twice more, once with cues on.
- **Left open, for the play test (issue 03):** the extra second word of a hold, "No. of CPs" read as "no", and the quarter second itself.
