# 04 The glide, the pad hook and steps over the portraits

Status: ready-for-agent
Type: task
Blocked by: 01

The first slice of stage 2 of `.scratch/character-select/spec.md`: the D-pad moves the hand from portrait to portrait. It proves steering end to end on the simplest targets. Read the spec's sections "The rule that keeps online play compatible" and the three "Stage 2" sections first.

## The rule

The fork writes nothing but port 1's controller input. No write to the hand's position or to any other game state, in this issue or any later one.

## Machinery

- **Pad hook.** `pc_a11y_pad(PADStatus*)`, declared in `a11y_hooks.h`, called in `publish_locked` (`src/pc/keyboard.c`) before the pad is published. It lets the fork set the stick of port 1's virtual pad, and its return value keeps the pad published when no other source is active. One line in the base port file.
- **The glide.** `steering.cpp`, pure: the hand's position, the destination, and the stick value the game applied this frame go in; the stick value to ask for comes out. It knows the game's movement formula and nothing about character select.
- **Per simulated frame.** The glide is advanced in the hand hook, once per simulated frame, never per video frame.
- **The applied stick** is what the game read for the hand's port this frame (`HSD_PadCopyStatus`). Comparing it with what was asked for tells the glide how long input takes to arrive, and whether the player's own stick has taken over.
- **Returning to rest.** A requested stick value is published once, then dropped unless the next simulated frame asks again.
- **Threads.** Find out whether `publish_locked` runs on the thread that simulates. If not, the requested value crosses threads and needs to be handed over safely.
- **Clamping.** Measure what stick values below a full push arrive as in the game (`HSD_PadClamp` lies between). A bounded run that publishes each value and logs what arrives is enough. Record the result in the primer.
- **Steps.** D-pad presses are read from the game's view of the local hand's controller, so keyboard, gamepad and adapter all work. `css_targets.cpp` answers "what is next in this direction"; this slice needs the three portrait rows only.
- **Switches.** Steering follows the accessibility switch. `MELEE_A11Y_STEER=1` turns it on by itself for agent runs with speech off; `tools/a11y/drive.py` sets it.
- **Where steering is off.** Outside character select; while the rules screen, name entry or a name tag window is open; once the screen has begun to leave; while `pc_net_resim()` is true.

## Behaviour in this slice

Rules are in the spec, section "Stage 2: stepping". This slice covers:

- Left, Right, Up and Down between portraits, skipping locked ones, without wrapping.
- A press at an edge of the portraits: no movement, the current portrait's name again. Up from the top row and Down from the bottom row count as edges until issue 05 adds the other rows.
- The first press from the start position: to the nearest portrait in that direction.
- The announcement at the press; a press during a glide changing the destination.
- Hover announcements held back during a glide; arrival where announced is silent.
- A step naming its destination with a free hand too.
- The glide aiming the coin's position, the hand plus (2.7, -2.0), at the portrait's centre.
- A failed glide: "Could not reach Fox", logged.
- The player's stick taking over: the glide is abandoned.
- Each glide logged in the speech log: its destination, and on arrival the frames it took.

## Done when

- `cmake --build build` passes, and the unit tests pass with two new tests:
  - **The glide**, against a simulated hand that uses the game's formula and the measured clamping. Inputs are delayed by 0 to 10 frames; one case repeats a sample, one skips one. From every portrait to every other, the hand ends inside the portrait, away from its edge. The longest glide at no delay takes under 90 frames.
  - **Steps over the portraits**: every direction from every portrait; with locked portraits; with Luigi locked, which swaps two portraits; at each edge.
- A drive script under `tools/a11y/` enters VS. Mode, Melee, steps Up into the portraits, walks a row with Right and waits on each portrait's name in order, steps Down a row, presses A, and waits on the glide's arrival lines.
- A bounded `vs` run with `MELEE_A11Y_STEER=1` and D-pad presses fed by the driver logs no glide: steering is off in a match.
- A run with steering on, left on character select with no input, shows the stick at rest: `MELEE_INPUT_TRACE=1` logs no stick value.
- `python tools/check_style.py` passes.
- The footprint grep shows the pad hook.
- **ADR-0004** is written: free-cursor screens are steered through the controller, with the rejected alternatives the spec lists.
- The primer's "Read, don't steer" is reworded as the spec says.
- CLAUDE.md: one "Accessibility status" line; under "Online compatibility", the pad hook as the one place the fork writes; `MELEE_A11Y_STEER` under "Verification".

## Comments
