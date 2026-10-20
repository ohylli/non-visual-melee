# The Accessibility category is wired into the base port's menus

Status: accepted (2026-10-20)

The fork's settings appear as an Accessibility category in the launcher's Settings and in the port menu. The base port has no way to add a category: each one is written out in `resources/launcher.rml`, `resources/port-menu.rml` and `launcher.cpp` (`tab_ids`, the `page_focus` lists, the handlers matched by element id), and controller input reaches the classes directly, bypassing RmlUi. So the fork edits those three files, and its footprint there grows beyond one-line hook calls.

The edit is kept to the category's wiring, each line marked with an `a11y` comment: the category's id, its button and an empty page, one line per `page_focus`, and a one-line hand-off at the top of each handler. The fork builds the rows into the empty page from the accessibility settings model, so a new cue or a newly shown tuning value never touches a base port file. The base port changes these files often, so most base merges that touch the launcher will conflict there; the conflicts are small and mechanical, and a redesign of the menus costs redoing about twenty lines.

## Considered options

- **Inserting the category at runtime** from the fork's RmlUi plugin, with about five one-line hooks for controller input. It keeps the isolation rule, but the base port would still hold its own category as selected, so the fork would have to hide and restore pages, catch Escape and the shoulder buttons, and follow the base port's internal state. It also breaks on a redesign, but silently at runtime, where the wiring breaks loudly as a conflict or a compile error.
- **Writing the rows into the base port's markup and code** like its own settings: the simplest code, but every row would be base port lines, conflicting at every merge and hand-written again for every new cue.
- **A separate page of the fork's own** with its own key and its own loop holding the game still: no base port lines, but the settings would sit outside the menus where players look for them.
