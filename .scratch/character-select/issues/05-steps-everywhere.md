# 05 Steps over the player slots and the top bar, and the sliders

Status: resolved (2026-09-30)
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

### 2026-09-30, implementation (agent)

Implemented; hearing it is issue 08.

- **Rows** (`css_targets.cpp`, `target_rows`): the top bar (Teams, Rules, Back as they exist), the unlocked portraits' rows, and in VS modes one row of the player slots, slot by slot: HMN/CPU button, team button in a team match for an open slot, CPU level knob for a CPU, handicap knob with the rule on for one's own slot or a CPU's. The HMN/CPU button stays in the row while it would not react (its coin in another hand, say), so the row does not shift under the player.
- **Where the hand is** (`locate`): the target among the rows whose area the game tests contains the hand, whether or not it would react now, so a hand carrying a coin over Back is on Back. A press steps from the glide's destination, or else from there; from nothing in the rows, as before, the nearest target that way. Up and Down from elsewhere now pick the row with the target nearest that way, so from the hand's start Up reaches player 1's button, not Pichu; `css_steps.drive` gained that step.
- **Nearest in x** compares the places a glide aims at (for a portrait, where the hand puts the coin at its centre), the same shift for every portrait, so steps among the portraits are unchanged.
- **A held slider.** Steering now runs while a slider is held. A step's destination is a value (`Target::level`): Left and Right one value, not past 1 or 9, Up and Down the same value, said again. The aim is the middle of the value's span, from the slider's lowest end, which `game_access.c` reads as the knob's world position less the joint's own translation (the game's `updateGrabbedSlider` arithmetic, worked backwards). Value changes during the glide go unsaid; a glide ending on another value says it. Grabbing or letting go of a slider during a glide stops it.
- **Coins.** Where a resting coin is lives only in the coin's struct, private to `mncharsel.c`, and it drifts, so a fourth hook, `pc_a11y_css_coin(slot, x, y)`, at the end of the coin's update (`fn_80262648`), reports it; the reader fills it into the snapshot. `pickable_coin` repeats the game's conditions (open slot, coin at rest on a portrait, a CPU's or one's own, the hand free), a CPU's first. The step says "Yoshi, player 2's coin" or "Fox, your coin", and the aim is the coin less (3.8, -2.6), kept below y 21 so a coin at the very top of the top row still leaves the hand in the portrait band. The glide's destination follows the coin (`Glide::retarget`), and a knob too.
- **A coin carried up into the top bar (the issue's question).** The game's carrying branch runs instead of the button tests: the coin stays in the hand, the top bar never reacts, A plays the refused sound, B puts the coin back. The step says so: "Back, not while holding a coin".
- **A coin carried down**: the step says what the coin going back does before the destination, "No character. Player 3: closed", or "Back to Yoshi. Player 2: CPU" for a CPU's coin; the drop on the way then goes unsaid. A later step or the end of the glide clears that, so a drop after a turned-back step is still said.
- **The HMN/CPU click** puts the hand at y -2.2, the button's middle, which is where the glide aims; team buttons likewise at -3.4. A click during a glide is tested at several frames and delays.
- **Wording**, for the play test to judge: "Teams: off, not while holding a coin" and "Yoshi, player 2's coin" join with a comma since they describe one target; "No character. Player 2: CPU" is two happenings.
- **drive.py** gained a `search RE BUTTON[*N] ...` step (press in turn until a line matches) and named captures: a group `(?P<cpu>...)` that a line matched is reused as `%cpu%` in later patterns, escaped. A new CPU's character is random, so `css_setup.drive` captures it from "Player 2: CPU, Kirby" and snakes over all 25 portraits until a step names that coin.
- **Runs.** `tools/a11y/css_setup.drive` passes: Fox for player 1, the top bar and Back's edge, player 2 made a CPU (Kirby that run), its coin found after 14 steps and picked up, carried to Ice Climbers and chosen, "Ready to fight. Press Start.", the CPU level knob, Left down to 1, four steps Right to 5 (each glide 2 frames), Up repeating "Level 5", "Released. Ready to fight. Press Start.", Start, "Stage select...", Start, and `boot scene: state 2 scene 2`, the match. A second run checked the top bar with a coin ("Teams: off, not while holding a coin", A refused), carrying one's own coin down ("No character. Player 3: closed", nothing more), and "Mewtwo, your coin" picked up where the glide aimed. `css_steps`, `css_portraits`, `css_slots`, `tree_walk` and `leaf_screens` pass; a bounded `title` run exits cleanly.
- **Unit tests.** `css_targets`: rows in a plain VS match, a team match, with the handicap rule on, in Stamina and Camera modes; along the top bar and the slots; Up and Down between rows of different lengths from each end and the middle; a slot becoming a CPU and one closing under the hand; a held slider from 1 to 9 and back and past each end; `locate`; which coin is pickable; the pickup aim. `steering`: every target to every other that is not portrait to portrait, at delays 0 to 10 (longest 54 frames at no delay); a coin's pickup point with the coin at the game's place and at its portrait's four corners; a held slider from every value to every other at delays 0 to 10, landing a quarter unit inside the value; a click on the HMN/CPU button during the glide. `css_speech`: the coin words, the top bar with a coin, carrying one's own coin and a CPU's down, a drop after a turned-back step, slider steps, a slider glide ending elsewhere. `launcher_data` fails as before on Windows. `python tools/check_style.py` passes.
- Not tried in a run: team buttons and handicap sliders by steps, a coin under two coins' pull, a failed glide, and online play (issue 06).
