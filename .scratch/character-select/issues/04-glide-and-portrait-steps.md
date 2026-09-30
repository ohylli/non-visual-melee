# 04 The glide, the pad hook and steps over the portraits

Status: resolved (2026-09-30)
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

### 2026-09-30, code review of issue 01 (agent)

Raised in the code review of issue 01 (commit `e10712e`) and left for this issue to consider: **move the hand hook's reports out of `hooks.cpp`**.

- **Now.** `hooks.cpp` owns `s_css_hands`, the per-hand buffer `pc_a11y_css_hand` fills, and clears it in `pc_a11y_scene_entered` next to `s_css_speech->forget()`. Forgetting character select's state therefore takes two edits there, and `a11y_game_css_state` takes the buffer as one of its 8 parameters.
- **Why not then.** The two resets sit side by side in the one function that means "a scene was entered", so they cannot drift apart, and a move would still leave two forget calls. Moving the buffer into `CssSpeech` was rejected: it would mix raw game input into the speech logic and stop the tests feeding it hand-written snapshots.
- **Why here.** This issue advances the glide in the hand hook and reads the applied stick per hand, so the raw per-hand state grows. That is the point where one small reader object pays off: it holds the reports (and whatever the glide needs per hand), has a single `forget()`, and hands the snapshot builder its state. `a11y_game_css_state` then loses its reports parameter. Decide while designing the machinery; skip it if the glide's state ends up living elsewhere.

### 2026-09-30, implementation (agent)

Implemented; hearing it is issue 08.

- **Threads (the spec's open question 5).** `publish_locked` runs in `pc_keyboard_apply`, called from `pc_frame_boundary` inside `VIWaitForRetrace`, on the game thread, and the pad alarm that samples the pads runs after it on the same thread. Nothing crosses threads; the requested stick is a plain member of the reader.
- **Clamping (open question 3).** Measured with a throwaway probe that published each value from the pad hook and logged `HSD_PadCopyStatus` in the hand hook, 296 values: every stick within a circle of 80 arrives as sent, down to 1; beyond it, scaled back onto the circle with each axis truncated (100,50 as 71,35; 64,64 as 56,56). The game sets the clamp's minimum to 0 (`gmmain.c`). Every value arrived exactly one simulated frame after it was published. Recorded in the primer's new section "How a stick value reaches a cursor".
- **The reader object** from the code review of issue 01 is `CssReader` (`css_reader.cpp`): it holds the hand reports, the speech, the glide and the stick request, and `forget()` clears all of it; `hooks.cpp` keeps no character select state. `a11y_game_css_state` still takes the reports, handed over by the reader.
- **The glide** (`steering.cpp`). Each simulated frame it predicts where the hand comes to rest from the sticks still on their way and asks for the stick that covers what is left, at most a full push. Near the end that is a small tilt, so the hand lands within a few hundredths of the aim point. It matches each applied stick against what it sent, which gives the delay, recognises a repeated or lost sample, and tells a stick it never asked for (abandoned). Two simulated frames in one video frame sample the pad twice; the pad hook tells the glide of every publish, so it counts the second sample as another push on its way. Without that, a repeat made the glide declare arrival while one push was still coming. Axis values of 1 and 2 are never asked for (the wire rounds them to 0), and a resting stick's drift within 14 per axis counts as the glide's own. It gives up after 180 frames.
- **Steps** (`css_targets.cpp`). Rows are built from the portraits' rectangles, so locked portraits and the Luigi and Pikachu swap need no special case. From a portrait: along the row, or to the portrait nearest in x in the next row that has one. From anywhere else, by the same rows: Up and Down to the nearest row that way, Left and Right along the row the hand is nearest to. A first version took the nearest portrait on that side in straight distance, and a run showed Right from left of the bottom row going up to Falco instead of along to Pichu. A press with nothing that way says "Nothing that way", a word the spec did not have, so that no press is silent.
- **Speech during a glide.** The hovers crossed go unsaid, and so does the frame after the glide ends. Then the hand's target is named if it is not the destination; "Could not reach Fox" comes first if the glide failed. What the hand does meanwhile is queued behind the step's name rather than cutting it off: stepping Up from the start, the coin jumps into the hand on the way, and "Holding your coin" follows "Pichu" without naming the portraits crossed.
- **Where steering is off**: outside VS modes (single-player modes read their hand from another port; issue 07), while the hand holds a slider, while the rules screen, name entry or the player's name tag window is open, once the screen begins to leave, when the hand stops updating (a glide then fails), and while `pc_net_resim()` is true.
- **Pad hook.** `any_active |= pc_a11y_pad(&st);` before the pad is published, plus the header include. The fork's stick is merged as the other sources are, each axis the one further from rest.
- **drive.py** sets `MELEE_A11Y_STEER=1`. It now looks for a button press among the buttons, sub-stick and triggers only, since the glide moves the main stick meanwhile.
- **Runs.** `tools/a11y/css_steps.drive` passes: Up to Pichu (24 frames from the start), Up to Fox and Mario, Right along the top row to Ganondorf with each name at its press, Right at the edge saying "Ganondorf" again, Down to Young Link (7 frames), A. `css_portraits.drive` and `css_slots.drive` still pass. A `vs` run with D-pad presses logs no glide. Character select left alone with steering on logs no stick off rest. A stick push during a glide abandoned it after 9 frames, and a step from where the stick left the hand glided to Pichu. The bounded `title` run exits cleanly.
- **Unit tests.** `steering` (new): the formula, every portrait to every other at delays 0 to 10, a repeated and a skipped sample at many points of the glide, drift, the player's stick, a new destination on the way, out of reach. The longest glide at no delay is 57 frames. `css_targets` adds steps from every portrait in every direction, edges, rows of different lengths, locked portraits, a fully locked row, Luigi locked, and steps from elsewhere. `css_speech` adds a step's words, silent crossing and arrival, ending elsewhere, a failed glide, a free hand gliding over a button, and the coin jumping in during a glide. `launcher_data` fails as before on Windows. `python tools/check_style.py` passes.
- Not tried in a run: a press during a glide (the driver's presses land after each 7-frame glide ends; the unit tests cover it), a failed glide, and online play at a real input delay (issue 06).
