# Port menu: speech, and drivers that reach the RmlUi pages

Status: agreed 2026-10-19 (design conversation with the maintainer)

The maintainer agreed:
- the scope: the port menu as it stands, with controller remapping and the fork's own settings left for later
- reading the page the way the launcher is read, wherever it has the same shape
- the opening and closing words
- silencing the cues while the menu is open
- the status line rule
- no extra words online
- the order of work: the test tooling first, proven on the launcher, then the port menu
- the tooling itself: pushed SDL events, a separate settings folder for every driven run, and a launcher mode for `drive.py`

The wording is a first proposal for the play test to judge.

## Goal

A blind player who opens the port menu (F1, or Back on a controller) hears where they are, moves through its tabs, settings and text, changes a value and hears the new one, hears the status line's news, and hears the game come back when the menu closes. The cues fall silent while the menu is open.

Before that, the agent gains a way to drive both RmlUi pages, the launcher and the port menu, so each change to their speech is checked by a drive run, not only by ear.

How the launcher is read is in `docs/a11y/rmlui-screens.md`, and why it is read that way is in ADR-0002. Vocabulary is from `CONTEXT.md`: port menu, launcher, setting, announcement, cue.

## Scope

In:

- **The drivers reach the RmlUi pages.** These pieces:
  - a fork-owned way to push SDL events into the game from a drive script
  - a settings folder of each run's own, so driven runs never touch the maintainer's settings
  - a launcher mode for `drive.py`
  - launcher drive scripts in the `verify` skill's menus level
- **The port menu speaks.** Its five tabs, their settings, sliders, text fields and lines of text, the status line, the update check, opening and closing.
- **Cues are silent while the port menu is open.**

Out:

- **Controller remapping.** It gets its own design conversation, covering:
  - the spoken names of inputs
  - the GameCube side's names
  - the bind prompt and its result
  - duplicate bindings
  - the port selector with the controller's name
  - cancelling a prompt from a gamepad

  Until then the general reader reads the Controls tab, usable but with SDL's raw words ("A, a, Enter to change").
- **The fork's own settings in the port menu** (the accessibility switch, the cue settings). This is a later feature, and their storage is revisited then.
- **The frame-rate counter** (`fps.rml`, shown by Performance display). It stays unspoken.
- **Online warnings.** Opening the menu in an online match freezes the player's game, and the other player's soon waits for it. The fork adds no words for this: the base port tells sighted players nothing either, and telling the other player would need a new message between the two copies, which breaks pairing with base port builds.

## What the port gives us

All of this is from reading `src/pc/launcher.cpp`, `resources/port-menu.rml`, `src/pc/vi.c`, `src/pc/keyboard.c`, `src/pc/main.c` and `extern/aurora/lib/dolphin/pad/pad.cpp` on 2026-10-19.

### The page

- `port-menu.rml` is built the way the launcher's Settings view is:
  - a tab strip (`tab-graphics`, `tab-audio`, `tab-cheats`, `tab-controls`, `tab-online`) whose pages are `page-*`
  - setting rows (class `setting`) with a heading, an optional help paragraph and a control
  - sliders with a `-val` text beside them
  - the Online page's text fields
  - a status line (`menu-status`) and a Return to game button (`resume`) under the pages
- **Rows with no control.** These are read as lines:
  - the "Sticks and triggers" heading and the Keyboard text on Controls
  - the connect code and the closing note on Online
- **The update check.** Its result is the help paragraph of the Check for updates row (`port-update-status`), rewritten as a check runs.
- **The controller's name.** Its row is the Controller row on Controls: the paragraph `pad-name` is the help text of the port selector (`pad-port`).

### The code

- **`PortMenu` mirrors the launcher's code.** The `PortMenu` class in `launcher.cpp` has its own `focus_ids`, `page_focus`, `focus_step`, `show_tab`, `step` (Left and Right: tabs, or the focused slider) and `apply` (Enter: cycle a value or act). The Online fields share the launcher's `type_online` and `change_online`.
- **Gamepad input bypasses RmlUi.** It reaches `PortMenu::gamepad` straight from SDL events:
  - D-pad or stick to move
  - A to choose
  - B or Start to close
  - shoulders to change tab
  - Back or Guide to open or close (Back only if no binding uses it)

  The keyboard reaches RmlUi:
  - Up and Down move
  - Left and Right on a tab change tab
  - Enter on a slider cycles its value, wrapping from the top value to the bottom
  - Escape closes
  - Tab is RmlUi's own document order today
- **The game stands still while the menu is open.** `VIWaitForRetrace` loops on `pc_frame_boundary` until the menu closes, so no game frame runs and no game hook fires. Two consequences:
  - The netplay silence rule (`pc_net_resim()`) cannot come into play while the menu is open.
  - **The cues keep sounding.** The tick hook stops, so the cue engine's voices carry on as the last tick left them.
- **Every change saves at once.** `PortMenu::saved` writes `launcher.cfg`, and a remap writes the controller bindings. Both live in the settings folder (`SDL_GetPrefPath(NULL, "melee-pc")`, under AppData on Windows). After a save the status line says "Saved." or the error.
- **The menu's frame.** `pc_menu_update` runs once per frame from `pc_frame_boundary` in `vi.c`, whether the menu is open or not.

### Reaching it from a drive script

- **`MELEE_KEY_FIFO` cannot reach the port menu.** It only sets the keyboard state the game polls (`keyboard.c`). The port menu and the launcher react to SDL events, which the game loop (`pc_frame_boundary`) and the launcher's loop (`Launcher::run`) both take from `aurora_update()`. So an event pushed with `SDL_PushEvent` should reach both pages as a real key would, whatever window has focus (open question 1).
- **Driven runs share the maintainer's settings folder.**
  - `drive.py` passes the disc on the command line, which skips the launcher.
  - Every run still reads the maintainer's `launcher.cfg`, so Unlock everything, for one, shapes what a run's character select shows.
  - Besides `main.c`'s `userPath`, three places take the settings folder from `SDL_GetPrefPath`: the net identity (`net_match.c`), the DHT node cache (`net_dht.c`) and the fork's cue settings (`cue_system.cpp`). `drive.py` already gives each run its own cue settings file through `MELEE_A11Y_CUE_SETTINGS`.

## Decisions

### The tooling

- **Pushed events (fork-owned).** A switch, `MELEE_A11Y_EVENT_FIFO` (the name is the implementer's to settle), names a pipe that a fork thread reads. The thread pushes real SDL events into the game:
  - key down and up (F1, the arrows, Enter, Escape, Tab)
  - typed text, for the text fields
  - gamepad button down and up, since neither page checks which device a press came from

  It lives in `src/pc/a11y/`, with no footprint. It runs only when the switch is set. It changes nothing a player's run does, and it never takes window focus.
- **`drive.py` steps for it.** Steps for a key, typed text and a gamepad button go through the new pipe beside the existing key pipe, so one script can mix game presses and RmlUi presses. The existing `press` step confirms a press by the game's pad log. That confirmation cannot work while the menu freezes the game, so the new steps confirm another way (open question 3).
- **A settings folder of each run's own (base port fix).** A `MELEE_USER_DIR` switch replaces the settings folder wherever the port uses it.
  - It goes in its own unmarked commit, since it helps every tester of the base port.
  - `drive.py` gives each run a fresh folder under `build/drive`, and `drive_net.py` gives each instance one.
  - A driven run then starts from default settings and never writes the maintainer's.
  - The fork's cue settings lookup follows the switch when `MELEE_A11Y_CUE_SETTINGS` is not set.
  - The base port build in `../melee-pc-base` lacks the switch, and as instance b of an online run it keeps reading the real folder.
- **A launcher mode for `drive.py`.** It starts the game without a disc argument, after writing into the run's fresh folder a `launcher.cfg` that names the disc, so the launcher opens on its Home page with the disc remembered. A launcher script never presses Choose disc: the Windows file dialog would take focus.
- **Verification.**
  - The `verify` skill's menus level gains launcher scripts and, later, port menu scripts.
  - Its "Out of reach" section shrinks to what the drivers still cannot do: raw gamepad input for remapping, and speech by ear.
  - CLAUDE.md's verification paragraph, which calls both pages out of the drivers' reach, changes with it.

### Hosting the port menu's speech

- **Two hooks in `PortMenu`, the same footprint as the launcher.**
  - A frame hook compares the page with the last frame, as `pc_a11y_launcher_frame` does.
  - A step hook is the first line of `PortMenu::focus_step`. It returns true when the reader used the press for a line of text, as `pc_a11y_launcher_step` does.

  Their exact place is open question 4.
- **A port menu reader beside the launcher's.**
  - `LauncherReader` (`launcher_speech.cpp`) mixes the general page reading with what only the launcher has: Home, the update banner, Back and the override table.
  - The general parts move to, or stay in, `rmlui_reader.cpp`, and a port menu reader is built on them.
  - The fork's RmlUi plugin starts each reader when its own document loads.
- **Silent while accessibility is off.** The step hook then leaves every press to the menu.

### Speech

Wherever the page has the launcher's shape, it reads like the launcher:
- setting rows as name, value, "Enter to change" and help
- sliders as their shown value, adjusted with Left and Right
- tabs as their name, "tab" and position
- Up and Down reading the lines between controls
- text fields with typing echo and refused characters explained
- Tab and Shift+Tab turned into Down and Up by the fork's capture-phase key listener
- the update check's result spoken as a watched region

Enter on a slider cycles and wraps (Master volume at 100% goes to 0%). The new value is spoken, and the fork leaves the behaviour alone.

What the port menu adds:

- **Opening.** When the menu opens, "PC settings" (the page's heading) followed by the focused tab's announcement: "PC settings. Graphics, tab, 1 of 5". It interrupts. The menu opens on the last tab used.
- **Closing.** On every way of closing (Escape, B, Start, F1, Back, Guide, Return to game): "Back to game", interrupting, so leftover menu speech stops as the game resumes.
- **The status line.** It is spoken queued once it settles, as the launcher's are. "Saved.", which follows every change, is never spoken. Errors, "Backend set to ... (restart required).", "Defaults restored." and the rest are spoken. How the bind prompt's texts ("Press a button on the controller. Escape cancels.", "Bound.", "Canceled.") fit in is for the remapping feature. Until then they are spoken like any other status text.
- **The Controls tab.** Read by the general reader, as above.

### Cues

- **Silent while the menu is open.** Every cue on the cue engine stops when the port menu opens. The first match tick after it closes starts them again from the match's state. A cue played through the game's audio is a one-shot and needs nothing.

### Tests

- **A headless unit test of the port menu reader.** It loads the real `resources/port-menu.rml` the way `test_launcher_speech.cpp` loads the launcher, through copies of `PortMenu`'s own helpers (`label`, `show_tab`, `focus_step` with its `page_focus` lists). It covers:
  - the opening and closing
  - every tab's first control
  - a setting row, a slider, a text field
  - the lines on Controls and Online
  - the status line rule, with "Saved." unspoken
  - the update check result
- **The launcher test still passes** after the reader is split.
- **Drive scripts.**
  - The launcher scripts check what the launcher's play-tested features say.
  - A port menu script opens the menu from the main menu, walks every tab, changes a value and back, reads a line, types in a text field, and closes.
  - A match script opens the menu mid-match with cues on and checks the cues stop and start again.

### Docs

- `docs/a11y/rmlui-screens.md`: the port menu beside the launcher. It covers the one-context note (no longer "everything except `launcher.rml`"), the hooks, the frozen game, and the status line rule.
- ADR-0002: an amendment. The port menu reuses the reader with its own two hooks. The forecast that it "must honour the netplay silence rule" does not apply, because the game stands still while it is open.
- A primer section on the tooling: how events are pushed, and why runs get their own settings folder. Probably `docs/a11y/rmlui-screens.md`'s "Testing without a screen", renamed.
- The `verify` skill, CLAUDE.md's "Driving the game" and "Verification", and an "Accessibility status" line.

## Open questions

For the implementer to find out:

1. Whether events from `SDL_PushEvent` reach RmlUi through aurora's input layer while the window lacks focus, and whether the port menu (`pc_menu_event`) and the launcher's loop see them as they see real ones. Which thread may push them (SDL allows any).
2. Which SDL event fields the pages and aurora read on a pushed gamepad button event (`which`, `button`), and whether a made-up device id is safe.
3. How a drive step knows a pushed event was taken while the game stands still and the pad logs nothing: a log line from the fork thread after the push, the next speech line, or a wait.
4. Where the port menu's frame hook sits: inside `PortMenu`, or in `pc_menu_update`, which runs every frame whether the menu is open or not. The reader must see the menu open and close (the document's visibility, or `pc_menu_is_open()`) to say the opening and "Back to game".
5. Which of the settings folder's users `MELEE_USER_DIR` covers (`main.c`'s `userPath`, the net identity, the DHT node cache), and whether one helper or a line at each. Also whether any drive script depends on the maintainer's settings and changes under a fresh folder: Unlock everything and locked tiles, the player name and the net identity in the LAN lobby, the stage select's Random roll.
6. What `launcher.cfg` needs so the launcher opens with the disc remembered (`launcher_data.cpp`'s format), and how a launcher script waits out the start-up disc check.
7. How the cue system learns the menu is open while no tick runs: from the port menu's frame hook, or from `pc_menu_is_open()` read where the fork already runs each frame.

## Issues

1. `issues/01-settings-folder.md`: `MELEE_USER_DIR`, and each driven run in a folder of its own.
2. `issues/02-pushed-events.md`: the event pipe and `drive.py`'s steps for it. Blocked by 01.
3. `issues/03-launcher-drive-scripts.md`: `drive.py`'s launcher mode and launcher scripts in the menus level. Blocked by 02.
4. `issues/04-port-menu-reader.md`: the shared reader, the port menu reader, its hooks and its unit test. Blocked by 03.
5. `issues/05-cues-silent-in-port-menu.md`: cues stop while the menu is open. Blocked by 02.
6. `issues/06-port-menu-drive-scripts.md`: port menu and mid-match scripts, the `verify` skill and the status line. Blocked by 04 and 05.
7. `issues/07-play-test.md`: the maintainer, by ear. Blocked by 06.
