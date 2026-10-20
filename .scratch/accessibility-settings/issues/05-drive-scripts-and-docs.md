# 05 Drive scripts, the verify skill and the docs

Status: resolved (2026-10-20)
Type: task
Blocked by: 04

`.scratch/accessibility-settings/spec.md`, "Tests" and "Docs".

## Done when

- The `verify` skill's menus level has a launcher script and a port menu script that open the Accessibility category, walk every row, change a switch, a volume and Ledge warning distance and back, press a Preview, and check the run's `a11y.ini` afterwards; and a run that turns Accessibility off and on again from the same row.
- `docs/a11y/rmlui-screens.md` covers the fork's own category: how its rows are built from the model and handed off, and the wiring's place in the footprint.
- CLAUDE.md gains an "Accessibility status" line for the feature.
- Every level of the `verify` skill the change calls for passes.

## Comments

### 2026-10-20, implementation (agent)

Implemented; the menus level has a script for the Accessibility category in each page, the drive scripts can check `a11y.ini`, and the docs and the status line name the feature.

- **The `ini` step.** `drive.py` gains `ini RE [SECONDS]`: it waits until a line of the run's accessibility settings file matches `RE` (5 seconds by default, `INI_TIMEOUT`), the run's own `a11y.ini` or the file a non-empty `MELEE_A11Y_SETTINGS` names, and logs `>> a11y.ini: <line>`; named groups are kept, as `wait`'s are. A read that meets the save's rename is tried again. The game saves before it logs `saved <key> = <value>`, so after a wait for that line the step finds the new line at once. In `drive.py --help` and the `verify` skill.
- **`launcher_a11y.drive`.** Launcher mode, `--muted`: Down five times from Graphics to the category, every row read in full in order (name, value, hint and help) and round to the first; Steering off by Right and on by Enter, Cue volume down and up by a step, Up from the first row wrapping to Ledge warning distance, up and down by a step, each change's `saved` line and its line in `a11y.ini` checked; Preview ledge warning played to its end (left then right), pressed again and stopped by moving off it; Accessibility off and on again from its own row; the file's comments and an untouched line still there; Escape twice to Home.
- **`port_menu_a11y.drive`.** From the main menu, `--muted`: F1, Up three times to the category, every row in full down to Resume; Speech off and on again from its own row; Cue volume by the gamepad's D-pad, Your position cue off and on by Left and Right, Preview your position cue by the gamepad's A, stopped by Escape to the category, Ledge warning distance down and up, each change checked in the file; Resume closing the menu. `run_level.py` fails it on the same status line texts as `port_menu.drive` ("Saved." never spoken).
- **Both run with `MELEE_A11Y_STEER=` and `MELEE_A11Y_CUES=` empty**, which `run_level.py` sets for them (`A11Y_ROWS`), so Steering and Cues come from the run's file and show no " (environment)" whatever the caller exports; they also pass with `MELEE_A11Y_CUES=1 MELEE_A11Y_STEER=1` exported.
- **What the runs showed.** A change logs `saved <key> = <value>` before the reader says the new value, so the scripts wait in that order. Turning Accessibility or Speech off, the switch's words come first and the row's new value "Off" after them, logged `(off)`, so unheard; both scripts wait on that `(off)` line. Turned back on, "Accessibility on" or "Speech on", then the row queued, and no value said. A second press of Preview starts the cue again after its short restart wait. In the port menu, Speech off and on mid-menu makes the main menu tree say its screen afresh ("Main Menu. 1-P Mode. Solo Smash!") right after "Back to game", since its reader forgot the screen when speech came back (issue 02's `MenuSpeech::forget`); the script does not wait on it.
- **Docs.** `docs/a11y/rmlui-screens.md`: a section "The Accessibility category" (the wiring in the base port's files and its place in the footprint, rows built from the shown list in the base port's classes, the hand-offs and which handlers need none, the fork writing its own values each frame, overrides, the port menu's sounds and status line, Preview), a sentence in the mental model and in "Testing without a screen". `CLAUDE.md`: an "Accessibility settings" status line naming the play test as waiting, and `MELEE_A11Y_CUE_PREVIEW` in the cue engine line, as issue 03 suggested. The spec's other docs were done by 01 to 04: `cues.md`'s "Accessibility settings", `speech.md`'s switches, CLAUDE.md's isolation rule, "Fork settings" bullet and match cues line, the `verify` skill's switches and `MELEE_A11Y_SETTINGS`. The `verify` skill now lists both scripts, the `ini` step, and the menus level's count of launcher and port menu scripts.
- **Verification.** Style; unit tests, all passing but `launcher_data` (known); a bounded `title` run; `run_level.py menus online`, 26 of 26, the two new scripts among them; the two again with `MELEE_A11Y_CUES=1 MELEE_A11Y_STEER=1` exported, 2 of 2; an inline run whose `ini` step looked for a missing line failed with `no line of ... matched '^nonsense$' in 1 s`. No C++ changed.
- **For the play test (06).** The main menu tree's screen said again after "Back to game" when Speech was turned off and on in the port menu: welcome or noise.
