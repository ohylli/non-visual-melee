# 04 The Accessibility category in the launcher and the port menu

Status: ready-for-agent
Type: task
Blocked by: 02, 03

`.scratch/accessibility-settings/spec.md`, "The Accessibility category", "Rows" and "Speech"; ADR-0007.

## Machinery

- **The wiring in the base port's files**, each line marked with an `a11y` comment and nothing more:
  - `tab-accessibility` in `launcher.cpp`'s `tab_ids`, before `tab-launcher`; the launcher's `page_focus` `case 5` becomes `case 6`
  - a category button and an empty page in `resources/launcher.rml` and `resources/port-menu.rml`
  - one line in each class's `page_focus` returning the fork's row ids
  - a one-line hand-off at the top of the handlers that act on an element id (spec open question 2), declared in `a11y_hooks.h`
- **The rows.** The fork builds them into the empty page from the model's shown list (issue 01) in the base port's markup classes, writes their values, saves through the model and plays the menu's own sounds as the base port's rows do. Switches step with Left, Right and Enter; sliders step with Left and Right by the value's step; Preview rows call issue 03's preview on Enter or A, and leaving the row or closing the menu stops it.
- **Speech.** The existing readers read the rows. Check that the switches' last announcements (issue 02) come out right from both pages, and that the status line rule still holds (spec open question 3).
- **Tests.** `test_launcher_speech.cpp`, `test_port_menu_speech.cpp` and `drive.py`'s `FOCUS_LINE` learn the new category.

## Done when

- In both pages, by keyboard and by controller (through the event pipe's `key` and `gamepad` steps), the category opens, every row is reached and changed, Preview plays, and `a11y.ini` holds the changes.
- The readers' headless tests cover the category's first row, a switch, a slider and a Preview row in both pages.
- `grep -rn a11y src CMakeLists.txt resources --exclude-dir=a11y` lists every changed base port line.
- The `verify` skill's menus level passes, and `python tools/check_style.py`.
- CLAUDE.md's isolation rule names the category's wiring as footprint, citing ADR-0007, and its "Fork settings" bullet describes the accessibility settings and `a11y.ini`.
