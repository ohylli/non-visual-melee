# The accessibility settings split into submenus inside the one category

Status: accepted (2026-10-30)

The Accessibility category had grown to one long column of every shown setting. It now holds only the master switches (Accessibility, Speech, Steering, Cues, Match announcements), each followed by a row that opens its submenu: Cue settings, and Match announcement settings with Voice settings nested inside it. The submenus are fork code inside the one category the base port's files already make room for ([ADR-0007](0007-the-accessibility-category-is-wired-into-the-base-ports-menus.md)), so a new submenu, or one nested deeper, adds nothing to the base port's files. The base port's footprint grows by one hook at the top of each page's Back (Escape or B): the launcher's Back already calls the fork first, but through the reader's hook that its Left calls too, and Left on a row holding a value must change the value, not leave the submenu.

Each submenu is a closed loop: Up and Down go round its own rows, and only Back (Escape, B, or Left on a row that holds no value) leaves it, to the row that opened it. The launcher reads its row list afresh on every press, so it walks a submenu's rows by itself; the port menu copies its row list once as a category opens, so inside a submenu the fork moves focus itself, through the step hook the reader already has. A loop needs no copy of the port menu's own rule that Down past the last row reaches Resume, a copy that would go stale unnoticed when the base port changes it.

## Considered options

- **A category of the sidebar for each part** (Accessibility, Match announcements, Cues): no new way of moving and nothing new for the readers, but about a dozen more lines in the base port's files, which conflict at base merges wherever the base port touches its categories, a few more for every later part, and no nesting.
- **Submenus that follow the port menu's own column**, leading past the last row to Resume as its pages do: the fork would copy the base port's movement rules and drift silently from them.
