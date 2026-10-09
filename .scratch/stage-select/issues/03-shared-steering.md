# 03 Shared steering: movement rule, step algorithm, pad dispatch

Status: ready-for-agent
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
