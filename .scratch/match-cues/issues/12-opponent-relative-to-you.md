# 12 The opponent's position cue relative to you

Status: resolved (2026-10-23)
Type: task

The spec left "the opponent panned relative to you" for after the play test. Two pans on one stage mapping say where each fighter is, but which side the opponent is on, compared with you, is hard to hear by comparing them. This issue adds a second mode to the opponent's position cue: pan and pitch say where the opponent is from you, not where they are on the stage. Your position cue stays as it is, the reference.

Decided with the maintainer on 2026-10-23.

## Machinery

- **The mode** is a shown switch in the opponent's group, a row of the Accessibility category beside the cue's own rows, off by default so the play-tested mapping stays the default.
- **Pan** from the horizontal distance to the opponent, `dx`: fully to that side at the side distance and beyond, moving in towards the centre as the opponent nears and stopping at a pan floor level with you, so their side is always heard. Linear in `|dx|`, as the ledge warning's pan.
- **Crossing**: the side flips only once the opponent is more than a crossing margin past you, so fighters overlapping or one standing over the other does not flicker the pulse from ear to ear. A new opponent, or the opponent back after a knockout, takes their side afresh.
- **Pitch** from the vertical distance: the cue's own pitch level with you, half the pitch range up at the height distance above you and beyond, half down as far below. The pitch range is the cue's own, as now.
- **Silent** as in the stage mode, and also while you are out: your position then is where you were knocked out, so a pan from it would mislead.
- **Values**: the switch is a setting; the side distance, the pan floor, the crossing margin and the height distance are tuning values in `a11y.ini`. The camera switch's description says it moves only your cue while the opponent's is relative to you.
- **Preview** in relative mode: a few pulses at the pan floor on the left, then a few on the right, level with you, so the floor itself is heard.

## Done when

- Unit tests for the relative driver: pan at and beyond the side distance, at the floor level with you, the crossing margin both ways, a fresh side after a change of opponent and after a knockout, pitch above and below, silent while you are out; the stage mode unchanged. The preview's two sides.
- The menus' rows test and the two Accessibility category drive scripts know the new row.
- A match run with the switch on in its settings file logs the opponent's cue on the correct side of you.
- `python tools/check_style.py` passes; `docs/a11y/cues.md`, the `verify` skill and CLAUDE.md's status line are updated.

## Comments

### 2026-10-23, implementation (agent)

- **The driver** is now a class, `OpponentPosition` (`position_cue.hpp`), since relative to you it keeps the opponent's side; the stage mode is unchanged. `opponent_side`, `relative_pan` and `relative_pitch` are pure functions beside it. `MatchCues::silence` forgets the side with the gap warning's hold.
- **Values** in the opponent's group: `relative` (shown, off), then the tuning values `side_distance`, `pan_floor`, `crossing_margin` and `height_distance`. Their starting values are in `OpponentMapping`, to be set by ear in issue 13. The row sits after Preview opponent's position cue, before the camera switch, whose description gained a sentence.
- **Tests**: `position_cue` gains the relative pan, crossing, pitch, silence and preview; `match_cues` the new keys and row, and a start logged from the settings' values with the camera switch both ways.
- **Drive runs**: `launcher_a11y` and `port_menu_a11y` read the new row (the port menu's script needed one more Down to reach its last row); the seven match scripts pass in the stage mode. `match_opponent.drive` run with `opponent_position.relative = on`: Mario fully right at GO, the pan falling as Link closes in, the stop at the bottom of the pitch range as Mario falls, the respawn fully left and high. Link running through Mario flipped the side once, just past the margin, with no flicker; the cue stopped as Link then fell off the edge.
