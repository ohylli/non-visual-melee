# 02 Tree screens speak

Status: ready-for-agent
Type: task
Blocked by: 01

The core of `.scratch/main-menu-tree/spec.md`: every tree screen of the main menu tree speaks, and every leaf screen says its name. Read the spec, ADR-0003 and the primer `docs/a11y/native-menus.md` first.

## Machinery

- **Frame hook.** `pc_a11y_menu_frame(void)`, declared in `a11y_hooks.h`, called once in `mnMain_Scene_OnFrame` (`src/melee/mn/mnmain.c`). Find out whether that function runs before or after the menu's objects update in a frame; either is fine, but the comment at the hook should say which.
- **Snapshot.** `game_access.c` fills a plain struct from game memory: `cur_menu` and `hovered_selection` of `mn_804A04F0`, and for the hovered entry its description number from `mn_803EB6B0`, or the base port's strings from `mnOnline_Label` and `mnOnline_Description`. Neither table is declared in a header, so `game_access.c` declares what it reads.
- **Menu reader.** Pure code that keeps the previous snapshot, compares, and composes the announcement. It speaks through the one `Speech` owned by `hooks.cpp`.
- **Forgetting.** The reader forgets its previous snapshot when `pc_a11y_scene_entered` reports any scene. Without that, returning from character select to the same screen and entry would look like no change.
- **Descriptions.** Decoded with `decode_game_text`, going through `pc_region_sis_index()` for the string's number. Wrapped lines are joined with a space. Unknown glyphs are left out of the speech and logged.
- **The old hook goes.** Remove `pc_a11y_menu_description` from `mnmain.c`, `a11y_hooks.h` and `hooks.cpp`. Keep `MELEE_A11Y_TEXT_DUMP=1` working from the frame hook.
- **Netplay gate.** Silent while `pc_net_resim()` is true, through the gate issue 01 added.

## Speech in this slice

Wording and rules are in the spec, sections "Tree screen announcements", "Leaf screens without a reader", "Where the words come from" and "The string table". In short:

- A tree screen opening: "Main Menu. 1-P Mode. Solo Smash!"
- The cursor moving: "VS. Mode. Multiplayer battles!"
- A leaf screen opening: "Rumble. No speech yet." This covers every leaf screen for now, including the three that issue 03 gives a reader.
- A missing name: "Unknown entry 3", "Unknown screen 27", logged.
- Online's labels in normalised capitals, and its title spoken as "Online".

## The string table

- Keyed by `MenuKind` and selection number, with the words of the spec's table.
- Take screenshots with the driver and check what the spec marks as unchecked: the entries of Melee Records, and the title of each leaf screen. Correct the table and the spec's table where the screen differs. Do not confirm into a screen where a blind press changes something: on Language, do not press A; on Erase data, do not press A.
- The three removed entries need no words.

## Done when

- `cmake --build build` passes, and the unit tests pass with a new test of the menu reader. It covers:
  - the first snapshot after a scene was entered, on the main menu and on a landing below it;
  - a cursor move, and a wrap from the last entry to the first;
  - Confirm into a tree screen and Back out of it;
  - a screen change and a cursor move in one step;
  - an unchanged snapshot, which speaks nothing;
  - a leaf screen opening, and the return from it;
  - an Online entry with the base port's strings;
  - a missing name;
  - a description with a line break.
- A drive script under `tools/a11y/` walks every tree screen and waits on each expected `[a11y]` line. It starts from `tools/a11y/main_menu.drive` and may grow out of `.scratch/main-menu-tree/drives/tree-walk.drive`. It enters Online and Melee Records as well, which the exploratory script did not.
- The same script, or a second one, leaves the menu into character select and comes back, and the landing is announced.
- A bounded `title` run exits cleanly.
- `python tools/check_style.py` passes.
- The footprint grep shows the new hook and no longer the old one.
- The primer is updated: the two choke points and the gaps of the description hook under "Candidate hook points" with a pointer to ADR-0003, locked entries being absent, and the answered open question about the poll.
- CLAUDE.md: one "Accessibility status" line; the drive script mentioned in "Verification"; the `--no-card` note about All-Star and Sound Test.

## Comments
