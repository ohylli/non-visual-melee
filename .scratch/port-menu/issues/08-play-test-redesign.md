# 08 Play test the readers on the redesigned pages

Status: ready-for-human
Type: task

The base merge of v0.4.0-beta (f4f4a4b) redesigned the launcher and the port menu after Melee's own main menu: a sidebar of categories, each opening a page of rows, two levels of navigation, and a description bar. The readers were adapted to it (`rmlui_reader.cpp`, `launcher_speech.cpp`, `port_menu_speech.cpp`, primer `docs/a11y/rmlui-screens.md`), with the wording the maintainer chose on 2026-10-20: a category says its name, position and description ("Graphics, 1 of 6. How the game looks."), with "Up and down to choose, right to enter" only on arriving in the sidebar; a setting row says "Left and right to change"; every control ends with the description the page shows for it; the launcher's Controls text is read with Down from its category. The unit tests and the drive scripts pass; nothing of it is heard yet.

## What to try

- Launcher Home: every button with its description, the opening, Verify Disc and Cancel Verify. Is a description after every Home button worth its length, the opening's included?
- Launcher Settings: entering it, every category and Back, Right or Enter into the rows and Escape, B or Left back out, a setting stepped with Left and Right, a slider, the Launcher category's rows, Controls' text from its category, the lines between Online's rows, the rows wrapping around.
- Port menu: the opening from a native menu and mid-match, every category and its rows, Resume, Quit game ("Quitting the game"), Escape on a category, the status line's news (Restore defaults, a binding prompt). Is anything said twice, now that the reader leaves out the status line's copy of each description?
- Port menu Controls: Input device stepped through the ports and Keyboard, with a controller connected and without. The binding rows say "-, Unbound" in a port with no controller, as the page shows them, and the Menu button row SDL's lowercase name ("back").
- Escape in an Online text field still does nothing and says nothing, on both pages (base port behaviour): is silence there a problem?

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue, and CLAUDE.md's Launcher and Port menu status lines no longer name this issue.

## Comments

2026-10-20: finding on the launcher's Controls page. Down from its category read the keyboard lines one by one, so reaching Online meant going through all of them, while one Up jumped back to the category. Fixed: Controls is now entered like any other category, with Right or Enter (A), which reads its first line; Up and Down go round its lines, and Escape, B or Left go back to the category, so Down on the category moves straight on to Online (hooks `pc_a11y_launcher_enter_text` and `pc_a11y_launcher_back`). The maintainer tested it by ear with NVDA and it works. The rest of the play test is still to do.

2026-10-20: finding on the categories of both pages. A category said its position among them ("Graphics, 1 of 6"), which no other reader does. Fixed: a category says its name and description alone ("Graphics. How the game looks."), and in the port menu its name alone ("Audio"). The maintainer tested it by ear with NVDA and it works.
