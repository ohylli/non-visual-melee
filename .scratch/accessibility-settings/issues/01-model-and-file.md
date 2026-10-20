# 01 The accessibility settings model and `a11y.ini`

Status: ready-for-agent
Type: task

`.scratch/accessibility-settings/spec.md`, "Storage" and "The model". The cue settings model (`value_model`, `value_file`, `cue_settings`) becomes the fork's whole accessibility settings model, kept in `a11y.ini` and written by the game one line at a time.

## Machinery

- **The file.** `a11y.ini` in the base port's settings folder, written fresh with defaults and descriptions when missing. `MELEE_A11Y_SETTINGS` replaces `MELEE_A11Y_CUE_SETTINGS`, with the same meaning for an empty value (defaults, no file). `a11y-cues.ini` is no longer read. `drive.py` and `drive_net.py` set the new variable.
- **The line edit.** A value set through the model is saved by replacing the value on its key's line, or appending the line under its group heading when the key is missing. Comments, order, other lines and unknown keys stay as written. The write goes through a temporary file and a rename. Spec open question 1: the poll that follows the game's own write finds nothing changed and says nothing.
- **`shown`.** A flag where a value is registered. The model lists the shown values in registration order, and for each cue registered with `add_cue` it lists the cue's switch, its volume and a Preview row (an action carrying the cue's id), so a front end needs no list of its own. Mark the values the spec's "Rows" table names, and register `position_cues.pan_to_camera` after the opponent's position cue.
- **Polling in the menus.** The file is also polled while the launcher or the port menu is open, from the hooks the fork already has there (`pc_a11y_launcher_frame`, `pc_a11y_port_menu_frame`).
- Names in code follow `CONTEXT.md`: accessibility settings, tuning value.

## Done when

- The `cue_settings` unit tests pass under the new names, with new cases for: the line edit keeping comments, order, other lines and unknown keys; a missing key appended under its group; the game's own write polled back silently; the shown list with each cue's three rows.
- A drive run with `MELEE_A11Y_SETTINGS` naming a file in the run's folder writes `a11y.ini` with every value, and a hand edit made while the launcher is open (`drive.py --launcher`) is picked up there.
- The `verify` skill's quick and menus levels pass, and `python tools/check_style.py`.
- `docs/a11y/cues.md`, "Cue settings", and the `verify` skill name `a11y.ini` and `MELEE_A11Y_SETTINGS`. CLAUDE.md's match cues status line names the new file.
