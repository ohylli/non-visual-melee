# Accessibility settings: master switches on top, the rest in submenus

Status: agreed 2026-10-30 (design conversation with the maintainer)

The maintainer agreed:

- Submenus inside the one Accessibility category, over a category of the sidebar for each part (ADR-0009).
- Each submenu a closed loop: Up and Down go round its own rows.
- Which settings go where, and in what order.
- The word "submenu" (`CONTEXT.md`), and what a row opening one is called and says.
- The keys that open and leave a submenu, where it opens and where Back lands.
- The way back to the top level when the menu closes.
- A title line for a sighted helper, which the reader leaves out.

The wording is a first proposal for the play test to judge.

## Goal

The Accessibility category had grown to one column of 36 rows. A player opening it now hears only the master switches, each followed by a row that opens its submenu:

- Accessibility
- Speech
- Steering
- Cues
- Cue settings
- Match announcements
- Match announcement settings

Everything else lives one level down, and the voices one level further, under the match announcements. The launcher's Settings and the port menu show the same layout.

Vocabulary is from `CONTEXT.md`. How the pages are read and how the category is wired is in `docs/a11y/rmlui-screens.md`; why submenus and not more categories is ADR-0009.

## Scope

In:

- **The layout.** Which row sits on which level, in what order.
- **The rows that open a submenu.** Their labels, descriptions and what the reader says for them.
- **Moving.** Opening, the closed loop, going back, and every way the category is left.
- **Speech.** What opening a submenu and going back say.
- **The title line.** What a sighted helper sees while a submenu is open.
- **One new hook in each page's Back.**

Out:

- **Any setting's own behaviour, label or help text.** Every row says what it says today, in its new place.
- **The file.** `a11y.ini`'s keys and values stay. Its order may follow the registration order if that has to change (open question 1), which a player never needs to know.
- **New settings.** None are shown that are not shown today.

## What the code gives us

All of this is from reading the code at commit `861eaefa` on 2026-10-30.

### The rows

- **One flat list.** `ValueModel::shown()` lists every shown value and action in registration order, which is also the file's order. `settings_page.cpp` builds the whole list into `#page-accessibility` as each document loads (`Page`), and `pc_a11y_settings_rows()` returns every row's id for both pages' `page_focus`. The hook takes no document.
- **Registration order today:** Accessibility; Speech, then both voices (`VoiceSettings`), then the match announcements (`MatchAnnouncementSettings`), registered inside the Speech switch's registration (`switches.cpp`, `speech_switch`'s `after_speech`); Steering; then `CueSettings` (Cues, Cue volume) and each cue from `match_cues.cpp`. The order the top level wants (Steering and Cues before Match announcements) is not that order.
- **The cue rows are already in the agreed order.** `add_cue` gives a cue its switch, volume and Preview; "Opponent's position cue relative to you" is registered right after the opponent's position cue, "Position cues follow the camera" after both position cues, and the ledge distance after the ledge warning.

### Moving in the two pages

- **The launcher re-reads its column on every Up or Down.** `Launcher::column` calls `page_focus()` each press, and `focus_step` skips a row that is hidden or disabled, wrapping round. So listing only the open submenu's rows is enough there.
- **The port menu copies its column once.** `PortMenu::show_tab` fills `focus_ids` from `page_focus()` as a category opens: its tab, the page's rows, Resume, Quit game. `focus_step` walks that copy without checking what is shown. Rows built or hidden later are never in it, so inside a submenu the fork has to move focus itself, in `pc_a11y_port_menu_step`.
- **That step hook is behind the speech gate.** `pc_a11y_port_menu_step` returns at once unless `game_hook_may_speak()`, and so does the reader's step. A submenu has to move with Accessibility or Speech off too, for a sighted player, so its step goes ahead of the gate, as `settings_pages_frame` already does in `pc_a11y_port_menu_frame`.
- **Back.** The launcher's `back()` (Escape, B) calls `pc_a11y_launcher_back()` first, but `horizontal` calls the same hook for every Left, so the hook cannot tell Escape from Left on a slider. The port menu's `back()` has no hook. So each page gets one new hook at the top of its `back()`.
- **Left and Right reach the fork first.** `pc_a11y_settings_step` is at the top of `Launcher::horizontal` and `PortMenu::step`. It returns false today for a Preview row, so the page's own Left goes back to the category; inside a submenu the fork handles Left there itself.
- **Enter, A and a click reach the fork** through `pc_a11y_settings_press` in `Launcher::action` and `PortMenu::apply`.

### The readers

- **A prefix exists.** `PageReader::follow_focus(focus, prefix, explain)` speaks a new focus with a prefix; the launcher uses it to say "Settings" as Settings opens.
- **The lines between rows are read with Up and Down** (`step_lines`, from both readers' step hooks). A title line drawn in the page would become such a line unless the reader leaves it out.
- **A row's spoken form comes from its markup** (`rmlui_reader.cpp`): a `button.value` is a setting, `button.value action` a button named by row and label ("Preview gap warning: Play, button"). There is no form yet for a row that opens something; the port menu's bindings show how a reader's `adjust` changes one ("Enter to change").

### Tests and scripts that walk the category

- **Headless:** `test_launcher_speech.cpp` and `test_port_menu_speech.cpp`, through `A11ySettings` in `test_rmlui_page.hpp` (the switches, the cues' shared values, the gap and ledge warnings, the ledge distance), with copies of both pages' `back`, `focus_step` and `page_focus`.
- **Drive scripts:** `launcher_a11y`, `launcher_settings`, `launcher_voices`, `port_menu`, `port_menu_a11y`, `port_menu_voices` and `match_thresholds` step down the category's rows by count.

## Decisions

### The layout

**Top level**, the Accessibility category itself:

| Row | Kind |
|---|---|
| Accessibility | switch |
| Speech | switch |
| Steering | switch |
| Cues | switch |
| Cue settings | opens a submenu |
| Match announcements | switch |
| Match announcement settings | opens a submenu |

**Cue settings:**

- Cue volume
- Your position cue, its volume, Preview
- Opponent's position cue, its volume, Preview, Opponent's position cue relative to you
- Position cues follow the camera
- Gap warning, its volume, Preview
- Ledge warning, its volume, Preview, Ledge warning distance

**Match announcement settings:**

- Voice settings (opens a submenu)
- Your damage, Opponents' damage, Damage interval, Your stocks, Opponents' stocks, Teammates, Time left

**Voice settings**, inside Match announcement settings:

- Your voice: Voice, Rate, Volume, Preview
- Opponents' voice: Voice, Rate, Volume, Preview

- **No repeats.** A master switch appears on the top level only, never again inside its submenu.
- **A new shown value says where it goes** when it is registered, so a new cue still lands in Cue settings with no other change.

### The rows that open a submenu

- **Labels:** "Cue settings", "Match announcement settings", "Voice settings". The plain names would clash with the switches.
- **Descriptions**, to start from:
  - Cue settings: "Cue volume, and each cue's switch, volume and preview."
  - Match announcement settings: "The voices, and what is announced: damage, stocks, teammates and the time left."
  - Voice settings: "Your voice and the opponents' voice: each one's voice, rate, volume and a preview."
- **Spoken:** the label, the hint, the description: "Cue settings, Enter to open. Cue volume, and each cue's switch, volume and preview." No role word: the hint already says what the row does.
- **They look like the base port's rows** for a sighted helper: a `row` with its name, and a control in the page's own classes.

### Moving

- **Opening:** Enter, A, a click, or Right on the row. The submenu always opens on its first row.
- **The closed loop:** Up and Down go round the submenu's own rows, from the last to the first and back, in both pages. Inside a submenu the port menu's Down never reaches Resume. The top level moves as the category does today.
- **Leaving:** Escape or B, or Left on a row that holds no value (a Preview, or a row opening a submenu), goes back one level, to the row that opened the submenu. Left and Right still change a value on a row that holds one.
- **The top level is unchanged:** Escape, B, or Left on a row holding no value go back to the category, as today.
- **Back to the top level** whenever the category is left any other way: the port menu closed (F1, Start, Resume, the menu button, Quit game), the launcher's Settings left, the page unloaded, or focus taken out of the page by the pointer. The next opening is at the top level, focus on the category as today.
- **With Accessibility or Speech off**, a sighted player moves through the submenus just the same.
- **A preview playing** stops as its row loses focus, as today, so going back stops it.

### Speech

- **Opening a submenu** says its label, then the first row's ordinary announcement, interrupting: "Cue settings. Cue volume, slider, 100%. Left and right to adjust." and its help. No key hint: Escape and B go back on every settings page.
- **Going back** is an ordinary focus move onto the row that opened the submenu: "Cue settings, Enter to open." and its description.
- **Moving and changing** inside a submenu are read as rows are read today.

### The title line

- **While a submenu is open**, a line at the top of the page names it: "Cue settings"; for the voices, "Match announcement settings / Voice settings".
- **The reader leaves it out** of the line reading: opening has already said it.

### The wiring in the base port's files

- **One new hook at the top of each page's `back()`**, `Launcher::back` and `PortMenu::back`, marked with an `a11y` comment, declared in `a11y_hooks.h`'s C++ section. It returns true when focus is in a submenu and the fork has gone back a level; the page then does nothing.
- **Nothing else changes there.** `page_focus`, `tab_ids`, the `.rml` files and the other hand-offs stay as they are.

### Tests

- **Headless, both pages:** opening by Enter, A and Right; the opening announcement; the loop at both ends; Escape, B and Left on a Preview back to the opening row, announced; Left and Right still stepping a value; the nested Voice settings in and out by two levels; closing the port menu and leaving the launcher's Settings from inside a submenu, the next opening at the top level; the title line never read as a line; moving with Speech off. `A11ySettings` gains what a nested submenu needs, and the copies of both pages' `back` the new hook.
- **Drive scripts:** each of the seven above reaches its rows through the submenus. The `verify` skill's menus level passes.

### Docs

- `docs/a11y/rmlui-screens.md`, "The Accessibility category": the submenus, how each page's moving is handled, the Back hook in the wiring list.
- `docs/a11y/cues.md`, "Accessibility settings", and `docs/a11y/speech.md`, "Choosing a voice": where the rows now are, and how a shown value names its submenu.
- `CLAUDE.md`: the Accessibility settings status line describes the top level and the submenus.
- The `verify` skill: the drive scripts' new paths.

## Open questions

For the implementer to find out:

1. **How a value names its submenu, and the top level's order.** Today one flat registration order is the row order and the file's. The top level wants Steering and Cues before Match announcements, which registration does not give. A shown value or a submenu could name its parent, with the rows of each level in registration order and the top level's order set where the submenus are declared; or registration could be reordered. The file's order matters only to a hand editor; the existing line edit keeps a player's file as it is.
2. **Which document `pc_a11y_settings_rows()` answers for.** It takes none. The launcher closes before the port menu loads, so one open submenu at a time may be enough; check before relying on it, or keep the open submenu per page.
3. **Where the port menu's submenu step sits** in `pc_a11y_port_menu_step`: ahead of the speech gate and of the reader's line step, so it moves with speech off. Check that the reader still says the focus it lands on.
4. **How the reader learns a submenu opened**, to put its label before the first row: the page could tell it, or the reader could spot the title line appearing as the launcher's reader spots Settings opening.
5. **The markup and the reader's form for a row that opens a submenu**, so it says "Enter to open" with no role word, and a sighted helper sees a row like the others.
6. **How the title line is left out** of `page_stops`.
7. **The pointer.** A click on a category or on Resume while a submenu is open must bring the category back to its top level; a click inside the open submenu works as today.

## Issues

1. `issues/01-submenus-in-the-page.md`: the layout, the rows that open a submenu, moving in both pages, the Back hooks, the way back to the top level, the title line; headless tests of focus.
2. `issues/02-speech.md`: the opening announcement, the opening rows' spoken form, the title line left out; headless tests of the words. Blocked by 01.
3. `issues/03-drive-scripts-and-docs.md`: the seven drive scripts, the `verify` skill, the docs and the status line. Blocked by 02.
4. `issues/04-play-test.md`: the maintainer, by ear. Blocked by 03.
