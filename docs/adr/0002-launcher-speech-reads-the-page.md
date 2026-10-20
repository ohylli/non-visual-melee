# Launcher speech reads the RmlUi page through a plugin and one frame hook

Status: accepted (2026-09-25), amended (2026-10-17, 2026-10-19)

The launcher (`src/pc/launcher.cpp`, `resources/launcher.rml`) is an RmlUi page that the base port changes almost daily: 39 commits in its first twelve days of history. CLAUDE.md prefers a hook at each semantic transition, but here the transitions (status text, focus moves, tab switches, view switches, each setting's value) are spread over a dozen private functions in that file. So the fork reads the page instead of being told about it. An `Rml::Plugin` registered from the existing `pc_a11y_init` hook learns when the launcher document loads and unloads, and listens to keys on it. A single hook, `pc_a11y_launcher_frame()`, called once per iteration of the launcher's own loop, compares the page with the previous frame: the focused element, its value, and a few watched text regions. The spoken words come from the page's own text, not from a string table. That gives one line of footprint in the busiest base port file, and new settings rows are spoken without fork work.

## Considered options

- **A hook at each transition** (`status()`, `show_tab()`, `show_settings()`, the focus moves, each value change): about ten lines in a file that changes daily, and every new feature in the launcher needs another.
- **A hook in `text()`, the launcher's one text-writing helper**: one line, triggered only by a real change, but it misses text written by other means (the input-delay value, anything a later data binding writes).
- **Zero base edits**: the plugin inserts an invisible fork element whose per-frame `OnUpdate` gives a tick. It works, but it adds a fork element to the base port's page and leans on RmlUi internals that a reader would not expect.
- **A fork string table keyed by element id**: full control of wording, but it must follow every row the base port adds. String tables stay for native menus, whose labels are images.

## Consequences

- Label reading depends on the page's markup shape (a setting row is a heading and help text beside a control). A base merge that restructures the page can silently break it. A headless unit test that loads the real `launcher.rml` guards against that.
- Changes are seen at most one frame late, which nobody hears.
- The port menu shares the RmlUi context and the row shape, so its feature can reuse the reader. It needs its own frame hook in the game loop, and must honour the netplay silence rule there.

## Amendment (2026-10-17): a step hook for a page's lines

Reading a Settings page's text line by line (`docs/a11y/rmlui-screens.md`, "Reading the lines of a page") needs Up and Down to stop where focus never goes, before the launcher moves focus. A frame comparison sees only where focus landed, too late to keep it. So a second hook, `pc_a11y_launcher_step(direction)`, is the first line of the launcher's `focus_step`, where every way of moving ends; it returns true when the fork used the press, and the launcher then leaves focus alone. That is a second line of footprint in the launcher, not a hook at each transition: everything else is still read from the page.

- **Text rows in the launcher's focus order** (`focus_ids`): the base port's order and its focus highlight would change for every player, in the busiest file.
- **The fork's capture-phase key listener**, which already turns Tab into Down: no footprint, but it sees only keys. The D-pad and the stick reach the launcher from SDL and never pass through RmlUi.

## Amendment (2026-10-19): the port menu reuses the reader

The port menu (`resources/port-menu.rml`, the `PortMenu` class in `src/pc/launcher.cpp`) is read the same way, as the consequences above forecast. The general reading moved out of the launcher's reader into `PageReader` in `rmlui_reader.cpp`; the launcher's reader keeps what only it has, and a port menu reader beside it adds the menu's opening, its closing and its status line rule. Each is started by a plugin of its own when its document loads. The menu has its own two hooks, the same footprint as the launcher's: `pc_a11y_port_menu_frame(open)` once a frame in `pc_menu_update`, open or not, and `pc_a11y_port_menu_step(direction)` as the first line of `PortMenu::focus_step`.

- **The frame hook carries the open flag.** The reader must hear the menu open and close to say so. The document's visibility is a computed style that RmlUi refreshes only as it draws, a frame late; the menu's own flag is right on the frame it changes, and while the game stands still none of the game's own hooks run to tell the fork anything else.
- **The forecast that it "must honour the netplay silence rule" does not apply.** While the menu is open the game stands still (`VIWaitForRetrace` loops on the frame boundary until it closes), so no frame is re-run while it speaks. The hooks still pass the gate every game hook passes, which never silences them wrongly.
- **Considered: one hook in `pc_menu_toggle` for opening and closing**, beside the frame hook. A third line of footprint, and the toggle is reached from four places (F1 in `vi.c`, the gamepad, Escape, Return to game) where the flag the frame hook carries already covers them all.

## Amendment (2026-10-20): two hooks for a page of text alone

The redesigned launcher opens a category's rows with Right or Enter, and leaves them with Escape, B or Left. Its Controls category has no row, only text, so Right found nothing to enter, and the text could be read only by walking Down through all of it on the way to the next category. Entering the text as any page is entered needs to know of two presses the page itself does nothing with, or does the wrong thing with: Right or Enter that found no row, and going back while the text is read, which would otherwise leave Settings. So two more hooks: `pc_a11y_launcher_enter_text()` as the last line of `enter_rows`, reached only when no row could be focused, and `pc_a11y_launcher_back()` as the first line of `back` and of `horizontal` for Left, which returns true when the fork used the press. Focus stays on the category throughout, so the launcher's order is still the base port's.

- **The fork's capture-phase key listener**, as for the step hook: no footprint, but it sees only keys, and A, B and the D-pad reach the launcher from SDL.
- **Reading Controls' text with Down from its category**, which needed no new hook: what the first version did, and the play test found that the next category then lay past every line.
