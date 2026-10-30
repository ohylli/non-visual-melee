# 04 Play test: the accessibility submenus

Status: ready-for-human
Type: task
Blocked by: 03

The maintainer opens the Accessibility category in the launcher's Settings and in the port menu, with NVDA.

Check by ear:

- The top level: five switches and two rows opening submenus. Is it easy to tell "Cues" from "Cue settings", and "Match announcements" from "Match announcement settings"?
- "Enter to open" on those rows, and their descriptions.
- Opening each submenu: its name, then the first row. Enough to know where you are?
- The loop: Up and Down going round inside a submenu. Does the wrap, with nothing said at it, ever lose you?
- Going back by Escape, by B, and by Left on a Preview: landing on the row that opened it.
- Voice settings two levels down: in, out, and out again.
- Closing the port menu from inside a submenu, and the next opening at the top level.
- Whether any row's new place feels wrong, or a setting is hard to find.

## Comments

- 2026-10-30, code review of issues 01 to 03: in the port menu, opening a submenu and going back play only the menu's move sound, the same as a plain Up or Down, where entering a category plays its confirm sound and a value's change its own. Speech says the difference; with Speech off, or for a player who goes by the sounds, nothing does. If the play test finds that wanting, the page can play `MenuSound::confirm` on opening and `MenuSound::back` on going back (the base port's move sound would still follow the focus move). Left as it is until heard.
