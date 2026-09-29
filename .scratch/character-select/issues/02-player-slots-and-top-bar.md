# 02 Player slots, the top bar and Ready to Fight speak

Status: ready-for-agent
Type: task
Blocked by: 01

The rest of stage 1 of `.scratch/character-select/spec.md`: every target outside the portraits, the screen's state changes, and the other player online. Read the spec first; issue 01 built the hooks, the snapshot and the reader this extends.

## Machinery

- **Snapshot grows**: per player slot its team, CPU level, handicap, and whether a slider of it is held; the teams rule; the match type; the Ready to Fight flag (`mnCharSel_804D6CF7`); whether a name tag window is open; the rectangles of the HMN/CPU and team buttons; the positions of the slider knobs and name boxes, which the game reads from its models at runtime.
- **Extend the frame hook's arguments** if the snapshot needs more of the file's private state. It stays one call.
- **What is under the hand** is decided by the fork from the same rectangles and distances the game uses, since the game keeps no "hovered button" anywhere. The spec's target table has them. Keep this in `css_targets.cpp`, pure, so stage 2 can use it to build its rows.
- **Who acted.** A change to a player slot is the local hand's when the hand is on that slot's button, or holds its coin or slider. Those are always announced. Any other change is another player's and is announced only when `pc_net_active()`.

## Speech in this slice

Wording and rules are in the spec, section "Stage 1: announcements". This slice covers:

- A free hand entering a button: "Player 2: CPU", "Teams: off", "Rules", "Back".
- A free hand reaching a slider's knob: "Player 2 CPU level: 1".
- A player slot changing kind: "Player 2: CPU, Yoshi", "Player 3: closed".
- A slider grabbed, its value changing, released: "Holding the slider", "Level 4", "Released".
- The team changing: the team as drawn.
- The hand picking up a CPU's coin: "Holding player 2's coin".
- Ready to Fight appearing: "Ready to fight. Press Start." Disappearing: nothing.
- Online, the other player choosing a character or changing their slot, queued behind current speech: "Player 2: Fox".
- The rules screen and name entry opened from here: their name and "No speech yet.", as leaf screens say it. Coming back from them: the opening announcement again.

## Done when

- `cmake --build build` passes, and the unit tests pass, extended to cover:
  - each kind of button entered and left;
  - the three kinds of a player slot in turn, including the random character a new CPU gets;
  - a slider grabbed, moved over several levels, released;
  - a team match: the Teams button, a team button;
  - Ready to Fight appearing and disappearing;
  - another player's choice and slot change, offline (silent) and online (queued);
  - the local hand choosing a character for a CPU (silent, the game's announcer speaks).
- Unit tests of `css_targets`: a position inside each kind of target, on an edge, and in empty space; targets that do not exist in the current mode.
- The drive script of issue 01, or a second one, moves the hand down to player slot 2's button, presses A until the slot is a CPU, and waits on each announcement; then picks a character for both and waits on "Ready to fight. Press Start."
- `python tools/check_style.py` passes.
- The primer gains what this slice learned about the buttons and sliders.
- CLAUDE.md: the "Accessibility status" line updated.

## Comments
