# Accessibility settings: one model, one file, a category in the launcher and the port menu

Status: agreed 2026-10-20 (design conversation with the maintainer)

The maintainer agreed:
- the words: setting, accessibility settings, tuning value, cue volume (`CONTEXT.md`)
- one file, `a11y.ini`, written fresh, with no carry-over from `a11y-cues.ini`
- the game editing only the changed line of the file
- a `shown` flag deciding which values are settings, every cue getting its switch, volume and Preview rows by itself
- the switch hierarchy, every switch live, and the way back from a switch that silences the menu
- environment variables as per-run overrides
- the category wired into the base port's files (ADR-0007), placed before Launcher
- the rows and their labels
- what Preview plays

The wording is a first proposal for the play test to judge.

## Goal

A player opens an Accessibility category in the launcher's Settings or in the port menu and hears, among the other settings:
- the switches for accessibility, speech, steering and the cues
- the cue volume
- each cue's switch, volume and a Preview button that plays a short sample of it
- whether the position cues follow the camera
- how far from an edge the ledge warning starts

Every change takes effect at once and is kept in the fork's own file. The values not shown stay in that file as tuning values for developers. Showing one of them as a setting, or adding a new cue to the menu, is a small change in one place.

Vocabulary is from `CONTEXT.md`. How the launcher and port menu pages are read is in `docs/a11y/rmlui-screens.md`; how cues and their values work is in `docs/a11y/cues.md`.

## Scope

In:

- **The accessibility settings model and its file.** The cue settings model grows into the fork's whole model, in `a11y.ini`, written by the game as the menus change it.
- **Live switches.** Accessibility, Speech, Steering and Cues, all changeable while the game runs.
- **Preview.** A short sample of each cue on demand, in the launcher and in the port menu.
- **The Accessibility category.** In both the launcher's Settings and the port menu, built from the model, read by the existing readers.

Out:

- **Carrying over `a11y-cues.ini`.** The game writes a fresh `a11y.ini` with defaults. An old `a11y-cues.ini` stays on disk, unread.
- **Previews while adjusting a volume.** Only the Preview button plays a cue. Cue volume has no Preview button.
- **A global shortcut to turn accessibility back on.** The way back is in "Switches".
- **Developer switches as settings.** `MELEE_A11Y_SCREEN_READER`, `MELEE_A11Y_LOG`, the cue dump and trace, the event pipe and the text dump stay environment-only.
- **Controller remapping's words.** Still its own later feature.

## What the code gives us

All of this is from reading the code at commit `9ae864d` on 2026-10-20.

### The model and its file

- **A model meant for a menu already exists.**
  - `value_model.{hpp,cpp}`: `ValueModel` holds `NamedValue`s, each with a key, label, description, unit, kind (toggle or number), default, range and step. It holds the parser and the serializer.
  - `value_file.{hpp,cpp}`: `ValueFile` does the disk work. It writes the whole file with defaults and descriptions when the file is missing, appends missing keys at startup, polls the file's stamp twice a second, and announces lines it cannot read.
  - `cue_settings.{hpp,cpp}`: `CueSettings` registers the cue values (`add_cue` gives each cue `.on`, `.volume` and `.boost`) and pushes levels into the cue engine.
  - `match_cues.cpp`: registers the mapping values (pitch ranges, intervals, distances, `position_cues.pan_to_camera`, `ledge_warning.distance` and the rest).
- **The game never writes a changed value back.** `ValueModel::set` and `to_text` exist for a front end, but only tests call them.
- **The file is only re-read at scene entry and on match ticks** (`CueSystem::poll_settings`). The launcher, the native menus and the port menu see no hand edit until then.
- **The file is located in `cue_system.cpp:cue_settings_path`**, from `MELEE_A11Y_CUE_SETTINGS` or the base port's settings folder (`pc_get_pref_path`). `drive.py` sets `MELEE_A11Y_CUE_SETTINGS` empty, which means defaults and no file.

### The switches today

- **All environment variables, read once.** `speech.cpp:config_from_environment`, called from `pc_a11y_init`, fills `a11y::Config`, which `Speech` and `CueSystem::open` copy. The header says to swap that function's body once fork settings get storage.
  - `MELEE_A11Y`: accessibility and speech, default on.
  - `MELEE_A11Y_STEER`: can only force steering on; steering otherwise follows `MELEE_A11Y`.
  - `MELEE_A11Y_CUES`: off means `CueSystem::open` never creates the engine.
- **Steering off already frees the D-pad for quick chat**: `pc_a11y_chat_buttons` hands the D-pad back whenever `screen_steers()` is false.

### The cue engine

- **The stream is open from startup, before the launcher** (`pc_a11y_init` runs before `pc_launcher_run` in `main.c`), so a cue can sound in the launcher, the native menus and the port menu.
- **No way to play a cue outside a match.** `CueSystem` keeps its engine to itself, and only `MatchCues::tick` drives voices.
- **The port menu stops every voice on every open frame** (`pc_a11y_port_menu_frame` → `CueSystem::silence_for_port_menu` → `MatchCues::silence`, which calls the engine's `stop_all`). `scene_entered` also stops all. A preview would be cut off at once.

### The launcher and the port menu

- **No extension point.** Each category and setting is written out in `resources/launcher.rml` and `resources/port-menu.rml`, in `launcher.cpp`'s `tab_ids` array, in the `page_focus` switch of `Launcher` and of `PortMenu`, and in each class's action, cycle, slider and refresh code, matched by element id.
- **Controller input skips RmlUi.** `Launcher::run` and `PortMenu::gamepad` call the classes' own `horizontal`/`step`, `activate_focus` and `back`.
- **The base port's storage cannot hold fork keys.** `launcher.cfg` ignores unknown keys on load and rewrites the whole file on save.
- **`PortMenu` assumes Launcher is the last entry of `tab_ids`** (`show_tab` uses `std::size(tab_ids) - 1`).
- **The base port changes these files often**: 46 commits to `launcher.cpp` in its five weeks, two redesigns of the menus in the week before this spec.

## Decisions

### Storage

- **One file, `a11y.ini`**, in the base port's settings folder, holding the whole accessibility settings model. `MELEE_A11Y_SETTINGS` names another file, set empty for defaults and no file, replacing `MELEE_A11Y_CUE_SETTINGS`. The format stays `key = value` lines under `#` group headings.
- **A change from a menu edits only its own line.** The game replaces the value on that key's line, or appends the line under its group heading if the key is missing. Comments, order, the player's other lines and unknown keys stay as written. The write is safe against a crash halfway, as the base port's `launcher.cfg` save is (temporary file and rename).
- **Hand edits stay live everywhere.** The file is also polled while the launcher or the port menu is open, so an open menu shows a hand edit.

### The model

- **A `shown` flag where a value is registered** makes it a setting. Every other value is a tuning value.
- **Every cue gets three rows by itself**: its switch (`<cue>.on`), its volume (`<cue>.volume`) and a Preview button. A new cue registered with `add_cue` appears in the menu with no other change.
- **Rows follow registration order**, the file's order. The page has no group headings; each row's label names its cue. To move a row, move where its value is registered.
- **The new switches join the model** as the first group, ahead of `cues`. Suggested keys: `accessibility.on`, `speech.on`, `steering.on`. The key names are the implementer's to settle.
- **Boost and the mapping values stay tuning values.**

### Switches

- **Hierarchy.** Accessibility is above Speech, Steering and Cues. Cues is above each cue's own switch. A higher switch overrides the ones below it without changing their saved values, as `cues.on` already does.
- **Accessibility off** stops speech, the cues, steering and every reader at once. The fork's rows stay in the menus, so a sighted player can turn it back on.
- **Every switch is live.** No restart. The cue engine is always created; Cues off mutes it.
- **Turning off from the menu.** Turning Accessibility or Speech off says "Accessibility off" or "Speech off" as the last announcement. Focus stays on the row, so pressing it again turns it back on, and the row announces itself.
- **Steering off** leaves the D-pad to the game and to quick chat.

### Environment variables

- **A set variable overrides the file for that run** and is never written to the file. A menu change to an overridden setting applies and is saved as usual.
- **`MELEE_A11Y=1` forces accessibility on.** Today it only defaults it. This is the way back for a player who saved Accessibility off and starts the game silent. Editing `a11y.ini` is the other. Both go in the player-facing readme once one exists.
- **`MELEE_A11Y_STEER` and `MELEE_A11Y_CUES`** become overrides in both directions, `0` and `1`.

### Preview

- **About two seconds of the cue at a fixed, typical moment in a match**, at its current settings. Exact targets are set by ear:
  - Your position and the opponent's position: a few pulses from the centre at middle height, at their slow pace.
  - Gap warning: the hum, ramped in and out.
  - Ledge warning: a few pulses on the left, then on the right.
- **Pressing Preview again restarts it. Leaving the row stops it.** Closing the menu stops it.
- **It ignores the cue's switch, the Cues switch and the Accessibility switch, and honours every volume**: the port's master volume and mute, Cue volume and the cue's own volume.
- **It works in the launcher and in the port menu.** The port menu's silencing stops only the match's voices, so a preview plays over a silent match.
- **Online it changes nothing a peer sees**: it plays on the cue engine, outside the game's sounds.

### The Accessibility category

- **Wired into the base port's files, kept slim** (ADR-0007). The base port's files get only the category's wiring, each line marked with an `a11y` comment:
  - the `tab-accessibility` id in `tab_ids`, before `tab-launcher`
  - a category button and an empty page in each `.rml` file
  - one line in each class's `page_focus` asking the fork for its row ids
  - a one-line hand-off at the top of each class's action, cycle, slider and activate handlers, so the fork handles its own rows

  The fork builds the rows from the model into the empty page, writes their values, and saves. Adding a cue or showing a tuning value never touches a base port file.
- **Before Launcher.** In the launcher it sits after Online. In the port menu, where Launcher is absent, it is the last category before Return to game. Only the launcher's `case 5` changes number.
- **Rows use the base port's own markup classes** (`row`, `name`, `button.value`, `button.value action`, the slider and its `-val` text, `data-desc` for help), so the base port's styles and the fork's existing readers apply unchanged.

### Rows

In this order:

| Row | Kind | Key |
|---|---|---|
| Accessibility | switch | `accessibility.on` |
| Speech | switch | `speech.on` |
| Steering | switch | `steering.on` |
| Cues | switch | `cues.on` |
| Cue volume | slider, 0–100 % in steps of 5 | `cues.volume` |
| Your position cue | switch | `your_position.on` |
| Your position cue volume | slider | `your_position.volume` |
| Preview your position cue | button | |
| Opponent's position cue | switch | `opponent_position.on` |
| Opponent's position cue volume | slider | `opponent_position.volume` |
| Preview opponent's position cue | button | |
| Position cues follow the camera | switch | `position_cues.pan_to_camera` |
| Gap warning | switch | `gap_warning.on` |
| Gap warning volume | slider | `gap_warning.volume` |
| Preview gap warning | button | |
| Ledge warning | switch | `ledge_warning.on` |
| Ledge warning volume | slider | `ledge_warning.volume` |
| Preview ledge warning | button | |
| Ledge warning distance | slider, stage units | `ledge_warning.distance` |

- `position_cues.pan_to_camera` is registered after the opponent's position cue, so it sits next to the position cues.
- Help texts to start from:
  - Position cues follow the camera: "On, the cues pan across what the camera shows; off, across the whole stage to the side blast zones."
  - Ledge warning distance: "How far from the edge the warning starts, in stage units. Final Destination is about 170 wide."

### Speech

- The rows are read as the existing readers read any row: name, value, the hint, help.
- Each change is said as the new value, as for the base port's settings.
- Preview says nothing; the cue is the answer.

### Tests

- **Unit tests of the model and file**: the line edit keeps comments, order, other lines and unknown keys; a missing key is appended under its group; `shown` values list in order with the cue rows generated; overrides are applied and never written.
- **Unit tests of the switches**: the hierarchy, live changes, the last announcement before silence.
- **Unit tests of Preview** on the cue engine's mix: it plays with switches off, honours volumes, restarts, stops, and survives the port menu's silencing.
- **The readers' headless tests** (`test_launcher_speech.cpp`, `test_port_menu_speech.cpp`) gain the category, through their copies of `tab_ids` and `page_focus`.
- **Drive scripts** in the `verify` skill's menus level: open the category in both pages, change a switch, a volume and the ledge distance, press Preview, and check `a11y.ini` afterwards.

### Docs

- `CLAUDE.md`: the isolation rule names the category's wiring as footprint (ADR-0007); the "Fork settings" bullet describes the model and `a11y.ini`; a new "Accessibility status" line; the match cues line's file name.
- `docs/a11y/cues.md`, "Cue settings": becomes the accessibility settings, with the file, the line edit and `shown`.
- `docs/a11y/rmlui-screens.md`: the fork's own category, how its rows are built and handed off.
- `docs/a11y/speech.md`: the switches are settings now, with the environment as overrides.
- The `verify` skill: the new switches' names, `MELEE_A11Y_SETTINGS`, the drive scripts.

## Open questions

For the implementer to find out:

1. How the game's own write and the poll get along: the write changes the file's stamp, so the next poll re-reads it. It must find nothing changed and say nothing.
2. Where each hand-off sits in `Launcher` and `PortMenu`: `action`, `cycle`, `step_slider`, the Change event, `activate_focus`, `apply`, `step`, `drag`. The base port's `is_slider` matches ids, so it does not know the fork's sliders. Who plays the menu's own sounds on a fork row.
3. Whether the port menu's status line shows "Saved." after a fork change, as after a base port one. It is never spoken either way.
4. How "Speech off" is said last: the announcement must reach the screen reader before speech goes quiet. Whether the speech log goes on recording while speech is off, so drive runs can check it.
5. What the readers do when Accessibility comes back on in the middle of a page: announce the focused row.
6. Steering switched off in the middle of a glide: the generated stick must go back to centre that same frame.
7. Whether `launcher.cpp`'s refresh code needs a hand-off too, or whether the fork writes its rows' values itself on each change.

## Issues

1. `issues/01-model-and-file.md`: `a11y.ini`, `MELEE_A11Y_SETTINGS`, the line edit, `shown` and the generated cue rows, polling in the menus.
2. `issues/02-live-switches.md`: Accessibility, Speech, Steering and Cues as live values, with environment overrides. Blocked by 01.
3. `issues/03-preview.md`: playing a cue's preview, and the port menu silencing only the match. Blocked by 01.
4. `issues/04-accessibility-category.md`: the category in both pages, its wiring, its rows and their speech. Blocked by 02 and 03.
5. `issues/05-drive-scripts-and-docs.md`: drive scripts, the `verify` skill, the docs and the status line. Blocked by 04.
6. `issues/06-play-test.md`: the maintainer, by ear. Blocked by 05.
