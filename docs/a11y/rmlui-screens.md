# RmlUi screens: how the launcher and the port menu are read aloud

## Mental model

The launcher, the port menu and the frame-rate display are pages in RmlUi, a UI toolkit that works like a small web browser inside the game: each page is a document of elements (buttons, sliders, text) described in an `.rml` file and styled by an `.rcss` file. RmlUi draws the page itself, so Windows and the screen reader see nothing but pixels. The fork makes a page speak by reading it: it watches which element has keyboard focus, what text is on that element and around it, and a few text regions that report progress and errors, and turns what changed into announcements.

```
launcher loop --pc_a11y_launcher_frame()--> launcher reader --compare with last frame--> announcements --> speech
launcher Up/Down --pc_a11y_launcher_step()--> launcher reader --read a line?--------------^
game loop --pc_a11y_port_menu_frame(open)--> port menu reader --compare with last frame---^
port menu Up/Down --pc_a11y_port_menu_step()--> port menu reader --read a line?-----------^
RmlUi --document loaded / key pressed--> fork plugins --------^
```

Each page's code tells the fork only two things: "another frame went by" (for the port menu, with whether it is open) and "the player asked to move up or down". [ADR-0002](../adr/0002-launcher-speech-reads-the-page.md) records why the reader compares the page instead of being told more. Both pages are built alike (tabs, setting rows, sliders, text fields, a status line), so most of the reading is shared: `PageReader` in `rmlui_reader.cpp`. Each page's own reader adds what only it has: the launcher's in `launcher_speech.cpp` (Home, the update banner, Back, its override table), the port menu's in `port_menu_speech.cpp` (the opening, the closing, its status line rule, its override table).

## One context for every page

Aurora (the port's graphics and UI library) creates a single RmlUi context, named `main`, and every RmlUi page is loaded into it: the launcher first, then after it closes the port menu and the frame-rate display. Anything the fork attaches at context level sees all of them, so each reader is started for its own document only (`launcher.rml`, `port-menu.rml`) and checks that the focused element belongs to it. The frame-rate display (`fps.rml`) has no reader.

## How the fork attaches

- **One plugin per page.** RmlUi lets a program register an `Rml::Plugin`, an object that is told when contexts are created and documents load or unload. The fork registers one for each page it reads (`ReaderPlugin` in `rmlui_reader.cpp`, one in `launcher_speech.cpp` and one in `port_menu_speech.cpp`) from `pc_a11y_init`, which runs before Aurora starts RmlUi; registering early is allowed and a plugin still receives every later notification. On its document's load a plugin attaches the fork's key and typed-text listeners and starts the page's reader; on unload it stops them.
- **One frame hook.** `pc_a11y_launcher_frame()` is called once per turn of the launcher's own loop, just after the page is drawn. That is where the reader compares the page with what it saw last frame. It runs after drawing because RmlUi works out styles, and so which elements are visible, only while it updates the page for drawing. The loop runs before the game starts, so the netplay silence rule does not apply to it. The port menu's hooks are under "The port menu".
- **One step hook.** `pc_a11y_launcher_step(direction)` is the first thing the launcher's `focus_step` does, before it moves focus up or down. Every way of moving ends there: the arrow keys, Tab, the D-pad and the stick. It returns true when the reader used the press for the lines of text, to read one or to come back from them to the focused control, and focus then stays put (see "Reading the lines of a page").
- **Shutdown order.** The fork's shutdown hook runs before Aurora shuts RmlUi down. It unregisters the plugins before speech shuts down, so RmlUi's own shutdown never reaches them. The reverse order is safe too: RmlUi unloads every document first, which stops the readers.
- **Plugin lifetime.** RmlUi keeps a pointer to the plugin until it shuts down, so the plugin is a static object. Its event classes include `EVT_BASIC` even though it needs only document events, because RmlUi's shutdown unregisters only the plugins in that class.

## Where the words come from

The launcher's labels are real text, so the reader takes them from the page, not from a string table:

- A button says what it is: "Play Melee", "Choose disc".
- A setting is a row (class `setting`): a heading with the setting's name, a help sentence under it, and a control beside them. Most on/off and multiple-choice settings are buttons whose text is only the current value ("On", "Windowed"); pressing Enter moves to the next value. So a button inside a row is read as a setting: name from the row's heading, the button's text as the value, then "Enter to change" and the help sentence. A badge such as "RESTART" sits inside the heading and is read with the name.
- A slider's value is shown in a text span next to it, whose id is the slider's plus `-val`, and that text is what gets spoken ("80%", "Auto"). The slider's own value is a bare number.
- The settings tabs are buttons with the class `tab` in a strip; the selected one also carries `selected`, and the position ("1 of 5") is counted among the strip's tabs. A tab's page is found by id, `tab-online` to `page-online`, a convention the port menu shares.
- A row with no control (the connect code, the Controls page's text) is never focused. The reader reads it line by line with Up and Down instead (next section). The tab announcement of a page that opens with such rows, as Controls does, ends with a hint that there is text below; on a page that opens with a control, such as Online, the lines come in their turn between the controls.

The page cannot tell a button that cycles a value from one that acts, since both sit in a row with only a label. Each page's override table (`kActionButtons` in `launcher_speech.cpp` and in `port_menu_speech.cpp`) lists the ones that act, by id; they are read as buttons named by row and label: "Performance preset: Apply, button". A base merge that adds an action row needs a line there, or the row is read as a setting with the value "Apply".

The page also uses visual separators (" / ", " · ", " → ") that the reader turns into commas, and a line break reads as a comma too. In a page's lines, " · " and a line break end a line instead (next section).

## Reading the lines of a page

Focus reaches only controls, so the lines of text between them are a reading position the reader keeps itself, beside focus, which stays on the control next to the lines. The generic part is `PageLines`, `tab_page_stops` and `PageReader::step_lines` in `rmlui_reader.cpp`; each page's reader names the control its own order puts after the page (the launcher's Back, the port menu's Return to game) and says when the lines may be read at all (`LauncherReader::step`, `PortMenuReader::step`).

- **What a line is.** The page's rows in document order are its stops: a row with a control is that control, and a row without one gives a stop for each of its lines. A line is a heading, or a piece of a paragraph between line breaks and " · " separators, so the keyboard layout reads one key and its action at a time ("X Attack"). The launcher's order on a Settings tab is the tab, the page's controls and Back, and the port menu's the tab, the page's controls and Return to game. Each matches the stops as long as its `page_focus` in `src/pc/launcher.cpp` (the launcher's and `PortMenu`'s) lists a page's controls in document order, one per row, and the control on either side of a run of lines is enabled (the launcher's step skips a disabled one, and the lines past it with it; the port menu disables none).
- **Moving.** Up or Down from a control onto a line speaks the line and keeps focus where it is. Off the end of a run of lines, the press either returns to the control focus is still on, which is announced again, or is handed back to the launcher, whose own step from that control lands on the control past the lines. So focus never has to be moved by the fork, and the launcher's order stays the base port's.
- **What ends it.** Any focus change, as when Left and Right switch tabs, a value change on the focused control, since Enter still acts on that control while a line is read, and the line being read changing on the page. The next press starts from the control again.
- **Accessibility off.** The step hook leaves every press to the page.

A sighted helper sees the focus highlight stay on the control while lines are read; the fork leaves the page's look alone.

## Why the reader compares frames instead of listening to events

RmlUi does send events, but several changes the player needs to hear have none, or arrive at the wrong moment:

- **Gamepad input bypasses RmlUi.** The launcher reads the gamepad from raw SDL events and calls its own functions, so a setting changed with a gamepad button fires no RmlUi event at all.
- **Text changes fire no event.** The status line and every setting value are plain text that the launcher code rewrites.
- **Values can be stale when an event fires.** When a slider's `change` event fires, the text span showing its value has not been updated yet.
- **The launcher swallows the arrow keys.** Its key listener handles Up, Down, Left, Right and Escape and stops them there, so a listener further up only sees them in RmlUi's capture phase.

Comparing once per frame sidesteps all of that: by the time the frame hook runs, the launcher has finished reacting to the input, whatever its source, and the page shows the final state. Several focus jumps inside one frame collapse into the one that stuck.

What the reader compares:

- **The focused element.** A new one gets a focus announcement that interrupts current speech. Only controls count (buttons and form fields). The launcher disables a button by blurring it, which parks focus on its container; the reader ignores that and keeps the last control, so focus that comes back to the same button, as when the file dialog is cancelled, is silent.
- **The focused element's value.** A new value on the same element gets only the value, interrupting. A button whose label changes ("Verify disc" becoming "Cancel verification") counts as a value change.
- **A focused text field.** `FieldEcho` in `rmlui_reader.cpp` compares the field's value and cursor with the last frame: an edit speaks the whole value, a cursor move the character after the cursor. A typed character that leaves the field unchanged is explained, which needs to know a character was typed: the fork's plugin hands the reader each `textinput` event (see "RmlUi event facts worth knowing"). The reader never learns a field's rules: at the field's `maxlength` the field was full, and anything else refused was not allowed.
- **What makes refusals visible.** The launcher's `type_online` (a base port fix, since it also misleads a sighted player) refuses a typed character before the field takes it, so the field always shows what is stored. Pasted text is still checked only after it is in the field, as the whole value. On leaving a field, the launcher also writes the stored value back into it, which matters when the player name was emptied, as an empty name is never stored. If leaving changed the field that way, the reader speaks the field's name and what it holds before the next focus announcement: "Player name, OTTO. Friend's connect code, edit, blank".
- **Entering Settings.** When the Settings view's own `display` turns on, the focus announcement of the tab it lands on is prefixed with "Settings".
- **Watched text regions** (the status lines, the update check result, the update banner). A change is spoken queued, and only after the text has stayed the same for a short settle time, so passing messages like "Checking disc image..." are skipped. A text whose only change is its percentage is progress: it is spoken each time it crosses the next reporting step, never on settling. The text that starts a run of progress ("Verifying disc / 0%") counts as reported, since the button's new label has already said what began. Two regions showing the same text in one frame ("Checking for updates...", which Check now writes to both the main status line and the result under it) are spoken once.
- **Settings' own regions.** The Settings status line and the update check result are watched only while Settings is shown, on any tab, so a check's result is heard after the player moves on. Opening Settings rewrites both, so what they show as Settings appears is taken silently. "Saved automatically.", the Settings status line's resting text, is never spoken.
- **The update banner's own display.** Its appearance is announced, and its title and description are read, by the banner's own `display`, not by whether it can be seen: Settings hides the whole Home view, and coming back must not announce the banner again.

The opening announcement waits for two things: the status line to settle and focus to reach a control. At start-up with a remembered disc, the launcher checks the disc with every button but Quit disabled, so focus is parked until the check is over. If focus never reaches a control, the opening is spoken without it a second later.

## The port menu

The port menu (F1, or Back on a controller) is `port-menu.rml`, loaded hidden as the game starts (`pc_menu_init`) and shown and hidden by `PortMenu::toggle`, so its reader lives from the game's start to its end and speaks only while the menu is open.

- **Two hooks, as the launcher has.** `pc_a11y_port_menu_frame(open)` runs once a frame from the game loop's frame boundary, open or not, after the frame's events, so it sees the menu open and close. `open` is the menu's own flag (`pc_menu_is_open()`): the document's visibility is a computed style, which RmlUi refreshes only as it next draws, so on the frame the menu opens or closes it would still say the old state. `pc_a11y_port_menu_step(direction)` comes before the menu moves focus, where the arrow keys, Tab, the D-pad and the stick all end.
- **The game stands still while it is open.** `VIWaitForRetrace` loops on the frame boundary until the menu closes, so no game frame runs and none of the game's own hooks fire (scenes, menus, the match tick): the menu's frame is where the fork learns what happens meanwhile. Nothing is re-run then, so the netplay silence rule never comes into play (`hooks.cpp` says why its gate still stands). The cues learn of the menu from the same frame hook, ahead of that gate, and stand silent while it is open (`cues.md`, "Lifecycle and switches").
- **Opening.** "PC settings", the page's heading, then the focused tab's announcement, interrupting: "PC settings. Graphics, tab, selected, 1 of 5. Left and right to switch tabs". The menu opens on the tab used last.
- **Closing.** "Back to game", interrupting, on every way of closing, so leftover menu speech stops as the game resumes. The reader forgets the focused control, so the next opening says it again.
- **The status line rule.** The status line (`menu-status`) and the update check's result (`port-update-status`, the help text of Check for updates) are watched regions, in view on every tab while the menu is open. "Saved.", which follows every change, is never spoken, nor the line's first text, "Saved automatically."; errors, "Defaults restored." and the bind prompt's texts are. What both show as the menu opens is taken silently, since opening rewrites them.
- **The update check's result arrives late.** Check now writes "Checking for updates..." to both regions, and the base port writes the result only when the page refreshes, at the next saved change or opening, so that is when the result is heard (base port behaviour).
- **Controls.** Read by the shared reader: each binding row as a setting with SDL's own names, Restore defaults as a button, and the "Sticks and triggers" heading and the keyboard's lines as lines. Remapping is its own feature, still to be designed.
- **Escape in an Online field does nothing.** The field keeps the key, as in the launcher, so a player closes the menu from there by moving off the field first, or with F1 (base port behaviour).

## RmlUi event facts worth knowing

- `focus` and `blur` do not bubble, but a listener added in the capture phase still receives them, on every ancestor down to the element.
- Enter, numpad Enter and Space on a focused element become a `click`, as RmlUi's default action.
- Tab and Shift+Tab move focus in document order, which differs from the order the pages' arrow keys use. The fork's key listener, in the capture phase on each page read, turns Tab into Down and Shift+Tab into Up so both keys follow one order. Stopping the event there also stops RmlUi's default action, its own Tab navigation.
- `Element::Focus()` refuses an element whose computed `focus` property is `none`, and computed properties are refreshed only when the page updates. The launcher disables a button by setting that property, so a button enabled and focused in the same frame used to stay unfocused; `Launcher::enabled` now updates the document after enabling.
- Typed text arrives as a `textinput` event on the focused element. The text field inserts it from its own capture-phase listener, so a listener on the document in the capture phase sees it first and can stop it; the field's length limit (`maxlength`) is applied inside the field, with no event when nothing fits. Aurora also sends a line break as typed text after every Enter, which a one-line field ignores.
- A text node's text is set after it is created, so `OnElementCreate` in a plugin is too early to read it.

## Testing without a screen

RmlUi runs without a window when it is given a render interface that draws nothing, which is how RmlUi's own tests work. The port menu's test (`test_port_menu_speech.cpp`) loads the real `resources/port-menu.rml` that way, through copies of `PortMenu`'s helpers (`label`, `toggle`, `show_tab`, and `focus_step` with every tab's `page_focus` list, which must follow the menu's), and passes the open flag as the menu's frame does. The launcher's test (`test_launcher_speech.cpp`) loads the real `resources/launcher.rml` the same way, moves focus and rewrites text through copies of the launcher's own helpers (`text`, `enabled`, `type_online`, and `focus_step` with the Online tab's `page_focus` list, which must follow the launcher's), types and presses keys through the context, runs `Context::Update` and the reader's frame step, and checks the announcements. Time is passed in, so the settle rule is tested without waiting. Both are mainly guards for base merges: if the base port reshapes a page so that a setting's name is no longer found, the test fails before a player hears "On" with no name.

## How the drivers reach the pages

The drive scripts' usual input, the base port's key pipe (`MELEE_KEY_FIFO`, `src/pc/keyboard.c`), cannot reach an RmlUi page. It only marks keys as held in the keyboard state the game polls once a frame to build controller 1's pad. The launcher and the port menu never poll that state: they react to SDL events, which both the game's frame boundary (`pc_frame_boundary` in `src/pc/vi.c`) and the launcher's loop (`Launcher::run`) take from SDL's queue through `aurora_update()`. Aurora routes key and typed-text events to RmlUi, and passes every event on to the loop, where F1 and the gamepad buttons are read.

So the fork has its own pipe, the event pipe (`event_pipe.cpp`), started as accessibility starts when `MELEE_A11Y_EVENT_FIFO` names one, with speech on or off. A thread reads requests from it (a key, typed text, a gamepad button) and pushes real SDL events with `SDL_PushEvent`, which any thread may call. `drive.py` gives every run one and sends its `key`, `type` and `gamepad` steps through it. What its making found:

- **No focus needed.** A pushed event reaches RmlUi and both loops while the window lacks focus, and pushing never moves focus. The base port's keyboard handler (`pc_keyboard_event`) refuses a key press without focus, so a pushed key never reaches the game's pad unless the window is focused, as a real key would not.
- **A key carries both codes.** RmlUi reads the keycode, the F1 toggle in `vi.c` the scancode, so the pipe sets both, the keycode from the keyboard layout as SDL's own events do. An event names no window, which aurora takes as its own window's.
- **Typed text points at memory SDL does not own.** SDL copies a pushed event but not its text, so the pipe keeps every typed character for the run. One event a character, as typing sends them.
- **A gamepad button names no device.** Its `which` is 0, an id SDL never gives a joystick. Aurora's input router looks the device up and drops the event, which RmlUi would ignore anyway; the launcher's loop and the port menu read only the button, so it works as a real controller's would. Pushing changes no gamepad state, so the game never sees the press either.
- **Taken, not just pushed.** The pipe pushes a marker event of its own behind each event and waits until it has left SDL's queue, which happens only when a turn of the loop polls past the event. That works while the port menu holds the game still, since its loop keeps polling, where the key pipe's confirmation, the game's pad log, shows nothing. The pipe then logs that the request was taken, and the drive step waits for that line. The events of one request go in one at a time, a short gap apart, so a press and its release, or two typed characters, fall in different turns of the loop as a person's would.

## How a drive run opens the launcher

`drive.py --launcher` starts the game with no disc argument, which opens the launcher (`src/pc/main.c`). Before the game starts, it writes a `launcher.cfg` into the run's own settings folder (`MELEE_USER_DIR`), in `launcher_data.cpp`'s format:

- **`disc` names the disc**, quoted as `std::quoted` writes it, so the launcher opens on Home with the disc remembered. At start-up the launcher checks that disc with every button but Quit disabled, and moves focus to Play once the check passes; the opening waits for both, so a script's first wait on the opening outlasts the check by itself.
- **`check_updates 0` turns the update check off.** With it on, the launcher asks GitHub for the latest release at start, and on a build older than that release Home shows the update banner, which the reader announces ("Update available: ..."). What a run says would then follow the network and whatever the base port last released.

The game rewrites the file at every start with all its settings, so a run's folder ends with a full one. Three more things shape a launcher run:

- **Accessibility on, screen reader muted.** The line reading (Up and Down over the text on a Settings tab) is one of the things the accessibility switch turns off, so a run with speech off moves straight from control to control. The launcher's scripts run with `MELEE_A11Y_SCREEN_READER=0` (`drive.py --muted`): accessibility stays on as for a player, and announcements are only logged.
- **Presses that would leave the window are refused.** Choose disc opens the Windows file dialog, which takes focus and cuts off the maintainer's screen reader; the Discord buttons and the update banner's open the browser; Display mode goes fullscreen. `drive.py` judges where focus is by the last focus announcement in the log and refuses a key or gamepad step that would press one of those (`REFUSED_FOCUS`), and in launcher mode any such press made before an announcement has said where focus is.
- **Play may wait for shaders.** With pipelines still compiling, Play says so in the status line and starts the game by itself once they are built, so a script waits for the game's first scene with a long timeout.
