# 01 Submenus in the Accessibility category

Status: ready-for-agent
Type: task

`.scratch/accessibility-submenus/spec.md`, "The layout", "The rows that open a submenu", "Moving", "The title line" and "The wiring in the base port's files"; ADR-0009.

## Machinery

- **The layout.** The top level holds the five master switches, with Cue settings after Cues and Match announcement settings after Match announcements; Voice settings is the first row of Match announcement settings. A shown value says which submenu it belongs to as it is registered, so a new cue lands in Cue settings by itself (spec open question 1).
- **The rows that open a submenu,** with the spec's labels and descriptions, in markup a sighted helper sees as a row like the others (spec open question 5; the spoken form is issue 02's).
- **Moving.** Enter, A, a click or Right opens a submenu on its first row. Up and Down go round its rows in both pages: the launcher through `pc_a11y_settings_rows()` listing the open submenu's rows, the port menu through the fork moving focus in `pc_a11y_port_menu_step`, ahead of the speech gate (spec open questions 2 and 3). Escape, B, or Left on a row holding no value goes back a level, to the opening row.
- **The Back hooks.** One new hook at the top of `Launcher::back` and of `PortMenu::back`, marked with an `a11y` comment, declared in `a11y_hooks.h`'s C++ section.
- **The way back to the top level** when the port menu closes, the launcher's Settings is left, the page unloads, or the pointer takes focus out of the page (spec open question 7).
- **The title line** at the top of an open submenu.
- **Tests.** In both readers' headless tests, by focus: opening by each key, the loop at both ends, back by Escape, B and Left on a Preview, Left and Right still stepping a value, the nested Voice settings two levels in and out, closing or leaving from inside a submenu and reopening at the top level, and moving with Speech off. `A11ySettings` in `test_rmlui_page.hpp` gains a nested submenu, and the copies of both pages' `back` the new hook.

## Done when

- In both pages, by keyboard and by controller (through the event pipe's `key` and `gamepad` steps), every row of every submenu is reached and changed, and every way back lands where the spec says.
- `grep -rn a11y src CMakeLists.txt resources --exclude-dir=a11y` lists the two new hook lines and nothing else new.
- The headless tests above pass, as does `python tools/check_style.py`.
- The drive scripts that step into the category may fail until issue 03; note which.
