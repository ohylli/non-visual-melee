# 02 Pushed SDL events for the drivers

Status: resolved (2026-10-19)
Type: task
Blocked by: 01

The second piece of the tooling in `.scratch/port-menu/spec.md` ("The tooling"). The launcher and the port menu react to SDL events, which `MELEE_KEY_FIFO` never sends. This issue gives drive scripts a way to send them.

## Machinery

- **A fork-owned event pipe.** A switch, `MELEE_A11Y_EVENT_FIFO` (name to settle), names a pipe that a fork thread in `src/pc/a11y/` reads.
  - The thread pushes SDL events with `SDL_PushEvent`: key down and up, typed text, and gamepad button down and up.
  - It starts only when the switch is set, adds no hook to a base port file, and never takes window focus.
  - It logs each push so a drive step can see it was taken (spec, open question 3).
- **Facts first.** Answer the spec's open questions 1 to 3 before building the steps:
  - whether pushed events reach RmlUi and the two pages' loops without window focus
  - which gamepad event fields are read
  - how a step confirms a press while the game stands still
- **`drive.py` steps.** A key, typed text, and a gamepad button, each through the new pipe beside the existing key pipe. Their names and syntax go in `drive.py`'s `--help`. `drive_net.py` passes them through to an instance as it does the other steps.

## Done when

- A drive run opens the port menu with F1 from the main menu, moves through it, and closes it with Escape. The same with a gamepad: Back to open, D-pad, B to close. Each is shown by a screenshot and by the base port's own log lines, since the port menu does not speak yet.
- A run that changes a port menu value leaves the maintainer's settings untouched (issue 01).
- `python tools/check_style.py` passes, and the `verify` skill's quick level passes.
- The `verify` skill lists the new steps where it covers drive scripts. CLAUDE.md's "Driving the game" names the switch and what it reaches.
- `docs/a11y/rmlui-screens.md` gets a short section on how the drivers reach RmlUi pages: pushed events, and why the key pipe cannot.

## Comments

### 2026-10-19, implementation (agent)

Implemented: drive scripts push real SDL events into the game, and reach the port menu and the launcher.

- **The pipe.** `MELEE_A11Y_EVENT_FIFO` (name kept), `src/pc/a11y/event_pipe.cpp`, started from `pc_a11y_init` whether speech is on or off, so no new hook. A detached thread reads one request a line, `ID key [Shift+|Ctrl+|Alt+|Gui+]NAME`, `ID text TEXT` or `ID gamepad BUTTON` (SDL's key and button names), and answers each in the log: `[a11y] event ID: <what> taken`, `... refused: <why>` or `... not taken in 10 s`. A key and a button are a press and a release; text is one event a character. Each event is pushed alone and waited on, then a short gap, so a press and its release, or two characters, land in different turns of the loop. Unit test `event_pipe` (the parsing).
- **The steps.** `drive.py`: `key KEY...`, `type TEXT`, `gamepad BUTTON...`, each request numbered and the step failing on any answer but `taken` or on none; `drive.py` sets the switch for every run (pipe `melee-events-<pid>[-name]` beside the key pipe). `drive_net.py` passes them through as it does every step; on a base port build b they fail ("the game never opened the event pipe (a base port build has none)"), so they go to `b-fork`. A wait after one of these steps finds lines from its first event on.
- **Open question 1, do pushed events reach the pages without focus: yes, both.** The port menu (F1 toggle in `vi.c`, RmlUi keys, `PortMenu::gamepad`) from the main menu, and the launcher's own loop (`Launcher::run`): a probe started the game with no disc, speech off but logged, and pushed Down, Down, Up, D-pad down and D-pad up on Home; each moved focus and launcher speech logged it ("Settings, button", "Discord, button", ...). So issue 03 can use the steps unchanged. Any thread may call `SDL_PushEvent`. A key event must carry the keycode (RmlUi) and the scancode (`vi.c`'s F1); the pipe takes the keycode from the layout with `SDL_GetKeyFromScancode`. The base port's `pc_keyboard_event` refuses a key press without focus, so a pushed key never reaches the game's pad in a driven run. Typed text: SDL copies the event but not its text, so the pipe keeps every typed character for the run.
- **Open question 2, gamepad fields: `type` and `button` only.** The launcher's loop and `PortMenu::gamepad` read nothing else; aurora's input router reads `which` to find the device and drops an event whose device it does not know before any layer, and RmlUi ignores gamepad buttons anyway. The pipe uses `which` 0, an id SDL never gives a joystick: safe. Pushing changes no gamepad state, so the game's pads never see it.
- **Open question 3, confirming a press while the game stands still: a marker event.** Behind each event the pipe pushes a marker of its own (`SDL_RegisterEvents`) and polls SDL's queue until the marker has left it, which happens only when a turn of the loop has polled past the event; the frame boundary keeps polling under the port menu, and the launcher's loop polls every turn. Then it logs `taken`. A speech line the press causes comes before the `taken` line (it is logged after the release).
- **Done when, keys.** From the main menu (`main_menu.drive`): `key F1` opened PC settings on Graphics (screenshot), `key Right Down Down Down Down` reached Audio's Mute audio, `key Return` set it On with the status line "Saved." (screenshot), `key Return` set it back, `key Escape` closed it (screenshot of the main menu), and `press Down` then got "VS. Mode. Multiplayer battles!", so the game ran again.
- **Done when, gamepad.** `gamepad back` opened it (screenshot), `gamepad dpdown dpdown` reached Vertical sync (screenshot), four `gamepad rightshoulder` and a `dpdown` reached Online's Player name, `type "ab#1"` put "AB" in the field ("ABPLAYER": the caret was at the start, `#` refused by `type_online`, `1` past the eight-character limit), `key Shift+Tab` moved focus to the Online tab, `gamepad b` closed it (screenshot), and `press Down` got the VS. Mode line again.
- **No base port log line shows the menu.** The base port logs nothing when the port menu opens, closes or saves, so the screenshots, the `taken` lines and the run's own `launcher.cfg` (`net_name "ABPLAYER"`) are the evidence until the menu speaks (issue 04).
- **The maintainer's settings stayed untouched.** Every file under `%APPDATA%\melee-pc` had the same modification time after both runs (a value changed in each) as before them; each run wrote its own `build/drive/<name>.log.settings/launcher.cfg`.
- **Checked.** Build, `python tools/check_style.py`, unit tests (`launcher_data` failing as known), a bounded `title` run without the switch (no pipe line) and with it naming no pipe (the thread retries quietly, clean exit), the menus level (17 of 17), and a `drive_net.py` run pushing events on a and b, then the same with `--base-b ../melee-pc-base/build` (b's steps skipped, b's unopened event pipe closed cleanly), both passing the session checks.
- **Docs.** `docs/a11y/rmlui-screens.md` "How the drivers reach the pages", CLAUDE.md's "Driving the game" and "Verification", the `verify` skill (the steps beside the menu drive scripts, the unit test, "Out of reach"), `drive.py --help`, `drive_net.py`'s docstring.
- **For issue 03.** A launcher run with a fresh settings folder checks for updates at start (`check_updates` defaults on) and, on this machine today, speaks "Update available: v0.3.1-beta" and the banner's text: either write `check_updates 0` into the run's `launcher.cfg` or let the scripts expect it. The launcher reads Shift+Tab through the fork's key listener; the port menu today takes RmlUi's own document order (issue 04).
- **For issue 04.** Moving focus into an Online field leaves the caret at the start, so typed text goes in front of what is there. Tab, typed text and the `taken` confirmation all work while the game stands still.
