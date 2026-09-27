# 02 Tree screens speak

Status: resolved (2026-09-27)
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

### 2026-09-27, implementation (agent)

Implemented; hearing it is issue 04.

- The reader is `MenuSpeech` in `src/pc/a11y/menu_speech.cpp`, the string table `menu_names.cpp`. The snapshot is `A11yMenuState`, filled by `a11y_game_menu_state()` in `game_access.c` with the same precedence as the game's own description line: the base port's plain text first, then SdMenu's string by number, read only while font slot 0 holds `SIS_MenuData`. The reader decodes a description only when it speaks it.
- `menu_kinds.h` mirrors `MenuKind` and the tree screens' selection enums the way `scene_kinds.h` mirrors the scene kinds; `game_access.c` checks every number at compile time, so a renumbering fails the build.
- `mnMain_Scene_OnFrame` runs before the menu's objects update in a frame (`gm_RunSimTick` calls the scene's frame function, then `HSD_GObj_RunProcs`), so the poll sees what the think functions left the frame before. The hook sits at the top, ahead of the L+R+Start check.
- The scene hook tells the reader to forget; the return from character select is announced ("VS. Mode. Melee. A standard Smash battle for 1 to 4 players.").
- Screenshots of every leaf screen and of Melee Records matched the spec's names except one: the random stage switch's title is "Random Stage". The spec's table notes are corrected. Snapshots shows a memory card prompt under `--no-card`, which its "No speech yet." does not cover.
- `tools/a11y/tree_walk.drive` walks all eleven tree screens with wraps both ways, Online, Melee Records, Rumble as a leaf screen, and character select and back: 96 announcements, no gap logged. The menu ignores input for its first 20 frames, so the script waits a second after arriving.
- Wrapped descriptions read as one line, for example "Single-Button Mode. The player uses only the A Button and the Control Stick. Great for beginners." No description on the tree screens has a font glyph, so no glyph was left out.
- `MELEE_A11Y_TEXT_DUMP=1` now dumps on the menu scene's first frame: 1604 strings, as before.
- The bounded `title` run exits cleanly. Unit tests: 9 of 9 pass, including the new `menu_speech`. `python tools/check_style.py` passes.
