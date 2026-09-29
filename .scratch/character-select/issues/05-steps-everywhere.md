# 05 Steps over the player slots and the top bar, and the sliders

Status: ready-for-agent
Type: task
Blocked by: 02, 04

Stage 2 of `.scratch/character-select/spec.md` for every target in VS modes. After this a blind player can set up and start a VS match alone. Read the spec's section "Stage 2: stepping" first; issue 04 built the glide and issue 02 the target geometry.

## Machinery

- **Rows.** `css_targets.cpp` builds all the rows from the snapshot: the top bar, the three portrait rows, the player slots. A target that does not exist in the current state is left out: the team button outside team matches, the sliders of a slot that is not a CPU, the handicap slider unless the rule is on, the Teams button in modes without teams, the rules header in Stamina mode.
- **Up and Down** go to the target nearest in x in the next row.
- **Destinations that move.** The slider knobs and the name boxes are where the game's models put them. The rows are rebuilt from each snapshot, and a glide's destination follows its target.
- **The HMN/CPU button moves the hand** to a fixed height when clicked. The glide treats that as having arrived.
- **A held slider.** Left and Right move one level, 1.25 units. The glide's destination is the centre of the level's span, so a frame more or less lands on the same level. Up and Down do nothing while a slider is held, and say so by repeating the value.
- **The name box** is not a target in this feature: the name tag window is out of scope.

## Behaviour in this slice

- Steps between all rows and along them, without wrapping, the name repeated at an edge.
- Stepping down out of the portraits with one's own coin in the hand clears the character, as the game does for any hand. Speech says "No character" and then names the destination.
- Stepping up out of the portraits into the top bar with a coin in the hand: the top bar does not react to a hand that carries a coin. Find out what the game does with the coin there, and announce the hand's state truthfully.
- A free hand stepping onto a portrait where a coin rests that it may pick up: "Yoshi, player 2's coin". The glide aims at the point from which A picks the coin up: the game takes the nearest such coin within 3 units of the hand plus (3.8, -2.6). With two coins on one portrait, the CPU's is preferred, since B calls one's own back from anywhere.
- Steps with a CPU's coin in the hand.
- A slider grabbed with A, moved by steps, released with A.

## Done when

- `cmake --build build` passes, and the unit tests pass, extended to cover:
  - the rows in a plain VS match, in a team match, with the handicap rule on, in Stamina mode;
  - Up and Down between rows of different lengths, from each end and the middle;
  - rows rebuilt when a slot becomes a CPU or closes while the hand is on it;
  - a held slider stepped from level 1 to 9 and back, and past each end;
  - the glide onto each kind of target at delays of 0 to 10 frames.
- A drive script under `tools/a11y/` does a whole setup by D-pad and buttons: chooses a character, steps to player slot 2, makes it a CPU, steps to the portrait its coin rests on, picks the coin up, carries it to another portrait and chooses, sets its level to 5, waits on "Ready to fight. Press Start.", presses Start, presses Start on stage select, and sees the match begin.
- The script also steps to each top bar target and back, without pressing A on them.
- `python tools/check_style.py` passes.
- CLAUDE.md: the "Accessibility status" line updated, and the new drive script listed under "Verification" among those to run after a change to menu speech and after a base merge.

## Comments
