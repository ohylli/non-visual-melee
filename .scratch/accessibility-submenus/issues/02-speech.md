# 02 What the submenus say

Status: resolved (2026-10-30)
Type: task
Blocked by: 01

`.scratch/accessibility-submenus/spec.md`, "Speech", "The rows that open a submenu" and "The title line".

## Machinery

- **Opening a submenu** says its label, then the first row's ordinary announcement, interrupting: "Cue settings. Cue volume, slider, 100%. Left and right to adjust." and its help (spec open question 4).
- **A row that opens a submenu** reads as its label, "Enter to open" and its description, with no role word, in both pages (spec open question 5).
- **Going back** is the ordinary focus announcement of the opening row.
- **The title line** is never read as a line by Up and Down (spec open question 6).
- **Tests.** Both readers' headless tests check the words: an opening row, the opening of each submenu including the nested one, going back, and Up and Down never stopping on the title line.

## Done when

- The headless tests above pass in both pages.
- A `drive.py --launcher --muted` run and a `drive.py --muted` run into the port menu log the opening and going back in the spec's words.
- `python tools/check_style.py` passes.

## Comments

### 2026-10-30, implementation (agent)

Implemented in both pages: a row opening a submenu says "Cue settings, Enter to open." and its description; opening one says "Cue settings. Cue volume, slider, 100%. Left and right to adjust." and its help, interrupting; going back says the opening row as any focus move.

- **Spec open question 5, the reader's form.** `describe_control` (`rmlui_reader.cpp`) gives a button of the class `submenu` a `Role` of its own, `submenu`, named by its row alone (its "Open" label is for the eye); `focus_announcement` says the name, "Enter to open" after a comma, as the bindings' "Enter to change", then the description. No role word. Same in both pages; the port menu's detail span under the name has no id, so it is not read twice.
- **Spec open question 4: the page tells the reader.** `settings_page.hpp` gains `SubmenuEntry`, which each reader holds and asks once a frame: it compares the page's open submenu (`Page::open_menu`) with the one at its last call and returns the new one's label when the category went into it, from the top level or from the submenu it is inside, and nothing when it went back up (the new one an ancestor of the old) or to the top level. The reader passes it as `follow_focus`'s prefix, which the launcher already used for "Settings"; the page focuses the submenu's first row within the same press, so the reader's next frame sees the new focus and the entry together. The title line was the other candidate; its text is for the eye and spotting it would mean parsing " / ", so the page's own state is asked instead. Voice settings says "Voice settings." alone, not the title line's whole path.
- **Asked every frame,** ahead of the readers' early returns, so an entry is said once. The port menu's reader runs behind the hooks' speech gate, so with Accessibility off it does not track; the first frame after the gate opens catches up (the switches that open it are on the top level), and at worst a submenu's name is said once with the row focus is on.
- **Focus put straight on a submenu's row**, as only a test can (hidden rows take no click), counts as entering it and says the submenu's name. Four existing cases that focus a submenu's row directly now expect that prefix (`accessibility_choice` in both suites, `accessibility_switch`, `accessibility_slider` and `accessibility_preview` in the port menu's); comments in the tests say so.
- **The title line** was already left out of `page_stops` in issue 01. The new `accessibility_submenu_words` case in each suite checks Up from a submenu's first row reads its last row, and Down round the whole of Cue settings moves focus at every press, each row said alone, the title line never a stop.
- **Tests.** `accessibility_submenu_words` in `test_launcher_speech.cpp` and `test_port_menu_speech.cpp`: an opening row's words; opening Cue settings by Enter and by Right; Up from the first row and the loop; back by Escape and by Left on a Preview, each the opening row; Match announcement settings, then Voice settings with its name alone; back, and in again from Match announcement settings with the name again; back two levels; and, after leaving the launcher's Settings or closing the port menu inside a submenu, the next opening at the top level with no submenu's name, a submenu entered then naming itself. The constants `kCueSettingsRow` and `kAnnouncementSettingsRow` carry the new form; `kVoiceSettingsRow`, `kLedgeDistanceRow`, `kTestPreviewRow` and an `opening()` helper are new.
- **Verification.** Build with the "Rollback sections" line; style; unit tests, all passing but `launcher_data` (known). The menus level: 29 of 33, the same four failing at the same first step into the old flat column as after issue 01 (`launcher_a11y`, `launcher_voices`, `port_menu_a11y`, `port_menu_voices`). By hand, two scratch drive scripts, `drive.py --launcher --muted` and `drive.py --muted` into the port menu from the main menu, both passing: "Cue settings, Enter to open. Cue volume, and each cue's switch, volume and preview."; Enter: "Cue settings. Cue volume, slider, 100%. Left and right to adjust. ..."; Up to "Ledge warning distance, ..." and Down back to "Cue volume, ..."; Escape to "Cue settings, Enter to open. ..."; Right in again; Left on "Preview your position cue: Play, button. ..." back; "Match announcement settings, Enter to open. The voices, ..."; Enter: "Match announcement settings. Voice settings, Enter to open. Your voice and the opponents' voice: ..."; Enter: "Voice settings. Your voice, Microsoft Zira, English (United States), Left and right to change. ..."; Escape, then gamepad A in and B out twice, each in the same words; Escape to the category; in the port menu Down to Resume and F1 "Back to game". The transcripts were removed.
- **The verify skill's** unit test line names the words now checked. Its script descriptions, the docs and the status line stay issue 03's; `docs/a11y/rmlui-screens.md` will want the `submenu` role and `SubmenuEntry` beside the Accessibility category's wiring.
- **For issue 03.** The drive scripts' new waits: an opening row is `"<label>, Enter to open\. <description>"`, and the first row of a submenu opened is `"<label>\. <its row>"`, so a script that steps into a submenu waits on the label before the first row (`"Cue settings\. Cue volume, slider, ...`, `"Match announcement settings\. Voice settings, Enter to open\. ...`, `"Voice settings\. Your voice, ...`), and every later row in the submenu without it.
- **Left open, for the play test.** Whether "Enter to open" suits a controller player, who presses A; the port menu's bindings already say "Enter to change" the same way.

