# 02 The drive script

Status: ready-for-agent
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
