# 03 Drive scripts, the verify skill and the docs

Status: resolved (2026-10-30)
Type: task
Blocked by: 02

`.scratch/accessibility-submenus/spec.md`, "Tests" and "Docs".

## Machinery

- **Drive scripts.** `launcher_a11y`, `launcher_settings`, `launcher_voices`, `port_menu`, `port_menu_a11y`, `port_menu_voices` and `match_thresholds` reach their rows through the submenus, and one of the launcher's and one of the port menu's opens and leaves a submenu, the nested one included, checking the spec's words.
- **The `verify` skill.** The scripts' new paths and expected lines.
- **Docs.** `docs/a11y/rmlui-screens.md`, "The Accessibility category": the submenus, moving in each page, the Back hooks in the wiring list. `docs/a11y/cues.md`, "Accessibility settings", and `docs/a11y/speech.md`, "Choosing a voice": where the rows are and how a shown value names its submenu. `CLAUDE.md`'s Accessibility settings status line.

## Done when

- The `verify` skill's menus level passes, and its online level, since `match_thresholds` sets values through the category.
- The docs name no row counts or values (CLAUDE.md, "Docs").

## Comments

### 2026-10-30, implementation (agent)

The drive scripts reach the Accessibility category's rows through the submenus, and the verify skill, the three primers and CLAUDE.md's status line describe the submenus.

- **`launcher_a11y.drive`**, the launcher's script that opens and leaves every submenu: the top level read in its order and round; Steering off and on there; Cue settings opened by Enter ("Cue settings. Cue volume, slider, 100%. ..."), every row and round to Cue volume without the name, Cue volume and Ledge warning distance stepped, Up from the first row wrapping to the last; Preview ledge warning played, pressed again and stopped by Left, which goes back to "Cue settings, Enter to open. ..."; Match announcement settings opened by Right ("Match announcement settings. Voice settings, Enter to open. ..."), every row and round, Your damage stepped; Voice settings opened by Enter ("Voice settings. Your voice, ..."), every row and round, Escape back to its row and Left on that row back to the top level; Accessibility off and on; Escape twice to Home.
- **`port_menu_a11y.drive`**, the port menu's: the top level down to Resume; Speech off and on; Cue settings opened by `gamepad a`, every row and round to Cue volume, never reaching Resume; Cue volume by the D-pad, Your position cue by Left and Right; Preview your position cue played by A and stopped by B, back to the opening row; Cue settings again by Right, Up wrapping to Ledge warning distance, stepped; Match announcement settings opened by Enter and read round; Voice settings by A, read round, Up wrapping to Preview opponents' voice, Left there back a level and B back to the top level; the menu closed by F1 from inside Cue settings and reopened by `gamepad back` on "PC settings. Accessibility. ...", Right landing on the top level's first row; Resume.
- **`launcher_voices.drive`, `port_menu_voices.drive` and `match_thresholds.drive`** take the new path: Down six from the Accessibility row to Match announcement settings, opened (Enter in the launcher, Right in the port menu), then Voice settings, its first row, for the voices; Down once past Voice settings for the damage rows. The voices' scripts take the same steps as before, so `run_level.py`'s sample counts stand. `port_menu_voices` now ends reading the match announcements' rows round to Voice settings, since Steering no longer follows them; `launcher_voices` and `match_thresholds` go back a level at a time with Escape.
- **`launcher_settings.drive` and `port_menu.drive` are unchanged.** Neither enters the category's rows: they pass over the Accessibility category in the sidebar alone, so the spec's "reach their rows through the submenus" has nothing to change in them. Both pass.
- **Docs.** `docs/a11y/rmlui-screens.md`: the Back hook in the wiring list, `pc_a11y_settings_rows` answering for the open level, a new "Submenus" section (every level built at once, the opening row's markup and the reader's `submenu` role, the title line and `page_stops` leaving out an `aria-hidden` row, opening with `SubmenuEntry`, moving in each page, going back, the way back to the top level, one page at a time), Left on a Preview, and the tests' copies of `back`. `docs/a11y/cues.md`: a new "Where a row goes" under "Accessibility settings" (`ValueGroup::in`, `add_menu`, a handle handed on keeping its submenu, `CueSettings::add_cue` and `add_shared`, `MatchAnnouncementSettings::voice_menu`, each level in registration order and the top level's from `pc_a11y_init`'s order), and the cue volume's and `add_cue`'s rows in Cue settings. `docs/a11y/speech.md`: the switches on the top level, and in "Choosing a voice" the voices' rows in Voice settings in place of the gone `after_speech` hand-off. No row counts or values named.
- **CLAUDE.md's status line** describes the top level and each submenu, names `pc_a11y_settings_back`, ADR-0009 and this feature's folder, and says the play test waits, `issues/04-play-test.md`.
- **The verify skill's** descriptions of the five changed scripts follow their new paths and words.
- **Verification.** Build with the "Rollback sections" line; style; unit tests, all passing but `launcher_data` (known). The menus level with the match announcements' set (`run_level.py menus match_damage match_thresholds match_stocks match_time match_stock_steal results_cpu4`): 39 of 39. The online level: `lan_lobby` failed once to find the other machine and passed on an unchanged rerun; `direct_back` and `direct_lost` failed twice at their `clipboard -$` wait, the clipboard holding "#SUBMENUS", which the skill names as a code on the clipboard, not a regression (the scripts change nothing on that path); the other six passed, `direct_call` among them.
- **Left open.** None of the scripts presses a click on a row, which the drivers cannot do; the headless tests cover the pointer.
