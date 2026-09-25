# RmlUi screens: how the launcher is read aloud

## Mental model

The launcher, the port menu and the frame-rate display are pages in RmlUi, a UI toolkit that works like a small web browser inside the game: each page is a document of elements (buttons, sliders, text) described in an `.rml` file and styled by an `.rcss` file. RmlUi draws the page itself, so Windows and the screen reader see nothing but pixels. The fork makes a page speak by reading it: it watches which element has keyboard focus, what text is on that element and around it, and a few text regions that report progress and errors, and turns what changed into announcements.

```
launcher loop --pc_a11y_launcher_frame()--> fork reader --compare with last frame--> announcements --> speech
RmlUi --document loaded / key pressed--> fork plugin -----^
```

The launcher code itself never tells the fork anything except "another frame went by". [ADR-0002](../adr/0002-launcher-speech-reads-the-page.md) records why.

## One context for every page

Aurora (the port's graphics and UI library) creates a single RmlUi context, named `main`, and every RmlUi page is loaded into it: the launcher first, then after it closes the port menu and the frame-rate display. Anything the fork attaches at context level sees all of them, so the reader checks which document an element belongs to and ignores the pages it does not handle yet. Today that is everything except `launcher.rml`.

## How the fork attaches

- **A plugin.** RmlUi lets a program register an `Rml::Plugin`, an object that is told when contexts are created and documents load or unload. The fork registers one from `pc_a11y_init`, which runs before Aurora starts RmlUi; registering early is allowed and the plugin still receives every later notification. On the launcher document's load the plugin attaches the fork's key listener and starts the reader; on unload it stops them.
- **One frame hook.** `pc_a11y_launcher_frame()` is called once per turn of the launcher's own loop. That is where the reader compares the page with what it saw last frame. The loop runs before the game starts, so the netplay silence rule does not apply to it; the port menu, which runs inside the game loop, will need it.
- **Shutdown order.** The fork's shutdown hook runs before Aurora shuts RmlUi down, so the plugin must cope with speech already being gone.

## Where the words come from

The launcher's labels are real text, so the reader takes them from the page, not from a string table:

- A button says what it is: "Play Melee", "Choose disc".
- A setting is a row: a heading with the setting's name, a help sentence under it, and a control beside them. Most on/off and multiple-choice settings are buttons whose text is only the current value ("On", "Windowed"); pressing Enter moves to the next value. The reader takes the name from the heading in the same row.
- A slider's value is shown in a text span next to it, and that text is what gets spoken ("80%", "Auto").
- The settings tabs are buttons in a strip; the selected one carries the class `selected`.
- A row with no control (the connect code, the Controls page's text) is never focused, so the reader speaks it as part of the tab announcement.

A small fork override table can replace wording that reads badly aloud. The page also uses visual separators (" / ", " · ") that the reader turns into pauses.

## Why the reader compares frames instead of listening to events

RmlUi does send events, but several changes the player needs to hear have none, or arrive at the wrong moment:

- **Gamepad input bypasses RmlUi.** The launcher reads the gamepad from raw SDL events and calls its own functions, so a setting changed with a gamepad button fires no RmlUi event at all.
- **Text changes fire no event.** The status line and every setting value are plain text that the launcher code rewrites.
- **Values can be stale when an event fires.** When a slider's `change` event fires, the text span showing its value has not been updated yet.
- **The launcher swallows the arrow keys.** Its key listener handles Up, Down, Left, Right and Escape and stops them there, so a listener further up only sees them in RmlUi's capture phase.

Comparing once per frame sidesteps all of that: by the time the frame hook runs, the launcher has finished reacting to the input, whatever its source, and the page shows the final state. Several focus jumps inside one frame collapse into the one that stuck.

What the reader compares:

- **The focused element.** A new one gets a focus announcement that interrupts current speech.
- **The focused element's value.** A new value on the same element gets only the value, interrupting. A button whose label changes ("Verify disc" becoming "Cancel verification") counts as a value change.
- **Watched text regions** (the status lines, the update banner). A change is spoken queued, and only after the text has stayed the same for a short settle time, so passing messages like "Checking disc image..." are skipped. A text whose only change is its percentage is progress: it is spoken each time it crosses the next reporting step.

## RmlUi event facts worth knowing

- `focus` and `blur` do not bubble, but a listener added in the capture phase still receives them, on every ancestor down to the element.
- Enter, numpad Enter and Space on a focused element become a `click`, as RmlUi's default action.
- Tab and Shift+Tab move focus in document order, which differs from the order the launcher's arrow keys use. The fork's key listener, in the capture phase, turns Tab into Down and Shift+Tab into Up so both keys follow one order.
- A text node's text is set after it is created, so `OnElementCreate` in a plugin is too early to read it.

## Testing without a screen

RmlUi runs without a window when it is given a render interface that draws nothing, which is how RmlUi's own tests work. The fork's test loads the real `resources/launcher.rml` that way, moves focus and rewrites text the way the launcher code does, runs the reader and checks the announcements. It is mainly a guard for base merges: if the base port reshapes the page so that a setting's name is no longer found, the test fails before a player hears "On" with no name.
