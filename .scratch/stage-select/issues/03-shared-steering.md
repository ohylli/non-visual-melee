# 03 Shared steering: movement rule, step algorithm, pad dispatch

Status: resolved (2026-10-09)
Type: task

Spec: `.scratch/stage-select/spec.md`, "Shared code". A refactor with one addition; character select's behaviour does not change.

- **Movement rule as a parameter** of the glide (`steering.cpp`): what a stick value moves the cursor in a frame, whether it moves at all, the stick that covers a distance, the full step. The hand's rule is today's (radial, quadratic). Add the stage cursor's: per axis, linear, dead zone 30, `0.03` per unit beyond it, at most 1.5. The glide's delay, repeat and loss handling, takeover, give-up, rest slack and wire rounding stay shared. Check that the aim tolerance suits both.
- **Generic step algorithm** out of `css_targets.cpp`: rows of targets, each with an anchor point; along a row, Up and Down to the nearest in x in the next row that has one, no wrapping, and the "from anywhere else" rule. Character select keeps its slider steps and coin-aware aim on top.
- **Pad dispatch**: `pc_a11y_pad` asks whichever screen's reader is live for its stick, instead of character select's alone. `pc_a11y_chat_buttons` ignores the D-pad on stage select too while steering is on, as it does on character select.
- The glide's status ("Could not reach", logging) stops depending on character select's target type.

## Done when

- `test_steering` runs every case against both movement rules: delays 0 to 10, repeated and lost samples, drift, the player's stick, a new destination, out of reach. Record the longest stage cursor glide at no delay.
- `test_css_targets` and `test_css_speech` pass unchanged, or with mechanical changes only.
- The character select drive scripts in the `verify` skill's menus level still pass.
- `python tools/check_style.py` passes.

## Comments

### 2026-10-09, implementation (agent)

Implemented; character select steers as before, and stage select has the pieces issue 04 plugs into.

- **Movement rule.** `MovementRule` in `steering.hpp`: the step a stick moves the cursor in a frame, whether it moves at all, the stick that covers a distance, the full step. `kHandMovement` is the old formula; `kStageCursorMovement` is per axis, dead zone 30, 0.03 per unit beyond, at most 1.5. The stage cursor's stick for a distance takes each axis on its own; a diagonal beyond a full push is taken back by the glide's shared code, the axis further out first, so both axes move about equally until the nearer is there, then the other at full speed (as quick as a straight line, within a frame or two). The glide's delay, repeat and loss handling, takeover, give-up, rest slack and wire rounding stay shared. The aim tolerance (0.05) suits both: the stage cursor moves in steps of 0.03 per axis, so it always ends within 0.015 on each.
- **A glide bug the stage cursor's tests found, fixed for both cursors.** The drift case now runs to every destination, not one diagonal. A straight full push of the glide's, say (0, 80), with a resting stick's drift merged into its other axis, (6, 80), is scaled back by the pad's clamp to (5, 79), and the glide took that for the player's stick and gave up. Two sticks both at a full push now count as the same within 2 on an axis. This changes character select only for a controller that drifts, where a glide straight up, down or along could stop after a few frames.
- **Steps.** `steps.hpp`, a header-only template over a screen's own target type: `row_step` (along a row, Up and Down to the nearest in x in the next row that has any, no wrapping) and `nearest_that_way` (from a point among no target). For stage select's tall tiles it already takes a target in several rows: Up leaves from its topmost row, Down from its bottommost, and Left and Right move along the row passed as `in_row` (the row the player came from), else its topmost. `css_targets.cpp` builds its rows with anchors (`placed_rows`) and keeps its slider steps and coin-aware aim on top. `test_steps.cpp` (new) covers the tall target and an empty row; `test_css_targets` and `test_css_speech` pass unchanged.
- **Steerer.** `steerer.hpp`: `Steerer<Destination>` holds the glide, `GlideState<Destination>` (phase and destination, what the speech follows), the stick until the pad hook takes it, and the two glide log lines, naming the destination through a words function the reader passes in. So "Could not reach" and the log no longer need character select's `Target`; `css_speech.hpp`'s `GlideStatus` is now an alias of `GlideState<Target>`. `pressed_direction` (the first D-pad press of an `A11yPad`) moved there too. `CssReader` keeps only what is its own: when steering is off, following a moving destination, and where a press goes.
- **Pad dispatch.** `pc_a11y_pad` asks the reader of the scene on now (`s_scene`: character select or stage select) for its stick; `pc_a11y_chat_buttons` drops the D-pad on both while steering is on. `SssReader` takes the steering switch and owns a `Steerer<int>` over `kStageCursorMovement`, destinations by tile index, words from `SssSpeech::tile_words`; it keeps the last snapshot for those words. Nothing starts a glide there yet.
- **Longest glides at no delay** (`test_steering`): the stage cursor 46 frames tile to tile, 54 from the start or a corner of its bounds to the far tile (about 50 units). The hand 46 portrait to portrait, 60 from a corner of its bounds (the old test's 57 was from its start to Ganondorf, now among these), and 54 to any target, as before. The stage cursor's longest glide is under a second, so 1.5 units a frame should feel brisk (spec open question 3, for the play test).
- **Docs.** ADR-0004's consequence about stage select's formula now names the movement rule; CLAUDE.md's character select steering line names the shared files.
- **Verification**, online level: build, style, unit tests (`steering` both rules, `steps` new; `launcher_data` fails as known), a bounded `title` run, all ten menu drive scripts, the five `css_net.drive` runs and the `lan_lobby.drive` run passed. A netplay run with screenshots after D-pad presses on character select and on stage select showed no chat line on either window.

For issue 04:

- **The applied stick.** `Glide::frame` wants the stick as the pad held it, to match against what it sent. `fn_8025A310` sees `mnStageSel_804D6CAC/AD` with the dead zone already taken off (an axis of 50 arrives as 20), so the hook should pass the stick before that, or the reader adds 30 back to a nonzero axis. Within the dead zone the cursor does not move and the glide takes the stick as at rest, so any value there will do.
- **The merged cursor.** Offline the first port whose stick is past 30 drives; port 1, steering's, is tested first, so another controller only moves the cursor while the glide's stick rests, and the glide sees that as the player's stick taking over.
- **The test cursor's start** is a guess, (0, -15), "below every tile"; the hook will show the real one. It does not change the longest glide, which runs from the corners.

