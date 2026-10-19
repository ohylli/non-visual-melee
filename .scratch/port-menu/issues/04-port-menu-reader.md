# 04 The port menu reader

Status: resolved (2026-10-19)
Type: task
Blocked by: 03

The speech of `.scratch/port-menu/spec.md` ("Hosting the port menu's speech", "Speech"). Read the spec first, then `docs/a11y/rmlui-screens.md` and ADR-0002.

## Machinery

- **The reader split.** The general page reading in `LauncherReader` (`launcher_speech.cpp`) moves to, or stays in, `rmlui_reader.cpp`. The launcher keeps only what is its own: Home, the update banner, Back, the override table. Its speech must not change, and the launcher's unit test and issue 03's scripts prove it.
- **A port menu reader** on the shared parts. The fork's RmlUi plugin starts it when `port-menu.rml` loads.
  - It reads as the spec's "Speech" says: the launcher's ways wherever the page has its shape, "PC settings" before the opening tab, "Back to game" on every close, and the status line spoken with "Saved." left out.
  - The Tab key handling covers this document too.
- **Two hooks in `PortMenu`,** declared in `a11y_hooks.h`: a frame hook (spec, open question 4) and a step hook as the first line of `PortMenu::focus_step`, returning true when the reader used the press. Every hook call is one line.
- **The usual gate.** Silent while accessibility is off. The netplay gate needs nothing extra while the game stands still, but the reader stays behind the same check the other game-time readers use, unless that check would silence it wrongly; say which in a comment.

## Done when

- `cmake --build build` passes. The unit tests pass, with the new headless test covering the spec's "Tests" list and the launcher's test unchanged or changed only in what it includes.
- A drive run with speech logged opens the port menu and hears the opening, a tab change, a setting, and "Back to game". Issue 06 writes the full scripts.
- The launcher scripts from issue 03 pass.
- `python tools/check_style.py` passes.
- `docs/a11y/rmlui-screens.md` covers the port menu, and ADR-0002 has its amendment (spec, "Docs").
- CLAUDE.md's grep inventory still finds every hook call.

## Comments

### 2026-10-19, implementation (agent)

Implemented: the port menu speaks, read the launcher's way on a reader the two pages share.

- **The reader split.** The general reading left `LauncherReader` for `rmlui_reader.cpp`: `PageReader` (focus and its value, the text field echo, the left-field rewrite, the tab hint "Text below, Down to read", the line reading as `step_lines`), `WatchedText` (the settle and progress rules; its one resting text became a list), `SpokenOnce` (a text two regions show at once said once), the Tab and typed-text listeners (`ReaderListeners`) and `ReaderPlugin<Reader>`, a plugin that starts a reader when its document loads. `launcher_speech.cpp` keeps Home, the update banner, Back, the Play disc card and its override table; its four functions and `kSettleTime` are unchanged in name and meaning, so `test_launcher_speech.cpp` is untouched and passes, and so do `launcher_home` and `launcher_settings`.
- **The port menu reader** (`port_menu_speech.cpp`), with a plugin of its own on `port-menu.rml` rather than one plugin for both pages: each page's file stays whole, and the launcher's test keeps calling only the launcher's functions. Opening: "PC settings. Graphics, tab, selected, 1 of 5. Left and right to switch tabs" (the page's `h1`, then the tab's announcement as the launcher says it, which the spec's example shortened), interrupting, on the tab used last; it waits up to a second for focus to reach a control, which in practice it already has. Closing: "Back to game", interrupting, and focus forgotten so the next opening names it again. Status line: `menu-status` and `port-update-status` watched while open, "Saved." and "Saved automatically." never spoken, what they show as the menu opens taken silently. Override table: Check now and Restore defaults are read as buttons ("Check for updates: Check now, button. Up to date (...)"); the binding buttons stay settings, as the spec's "A, a, Enter to change" has them.
- **The Tab key** turns into Down and Up on this document too (the shared listener), so Tab follows the menu's own order.
- **The hooks** (`a11y_hooks.h`): `pc_a11y_port_menu_frame(bool open)`, the last line of `pc_menu_update`, and `pc_a11y_port_menu_step(int direction)`, the first line of `PortMenu::focus_step` (as `if (...) return;`, the launcher's form). `hooks.cpp` puts both behind `game_hook_may_speak()`, with a comment on why that gate never silences them wrongly: a re-run frame is simulated inside a tick, never at the frame boundary, and while the menu is open nothing runs; under `MELEE_NET_SYNCTEST`, a debug mode, every game hook is silent, this one too.
- **Open question 4, where the frame hook sits: in `pc_menu_update`, passing the menu's own open flag.** `pc_menu_update` runs every frame from `pc_frame_boundary`, open or not, after the frame's events, so the reader sees the open and close transitions there. The flag, not the document's visibility: `Show` and `Hide` change a style that RmlUi computes only as it next draws, a frame late, while the flag is right on the frame it changes. Passing it, rather than the reader calling `pc_menu_is_open()`, keeps the reader free of the port's symbols, so the unit test drives it.
- **For issue 05 (open question 7): the menu's opening is seen in `pc_a11y_port_menu_frame(open)`.** It runs every frame boundary, open or not, also while the game stands still, so the cue system can learn of the menu there: in `hooks.cpp`, before the speech gate, something like `if (s_cue_system != nullptr) s_cue_system->port_menu(open);`. The pad hook (`publish_locked`) and the chat hook also run at each frame boundary while the menu is open, but neither says whether it is.
- **Unit test** `port_menu_speech` (`test_port_menu_speech.cpp`, the real `resources/port-menu.rml` with copies of `PortMenu`'s `label`, `slider`, `toggle`, `show_tab` and `focus_step` with every tab's `page_focus`): opening and closing, reopening on the last tab, Return to game, a closed menu silent, every tab's first control, a setting cycled with "Saved." unspoken, a slider stepped and wrapped, typing with a refused character and a full field, Controls' heading and keyboard lines, Online's connect code and closing note, the status line rule, the update check's result after moving on, Tab, accessibility off, every control named, the launcher's page ignored, unloading. A mutation (dropping "Saved." from the resting texts) fails it.
- **Done when, the drive run.** A hand-written `drive.py --muted` run from the main menu (not kept; issue 06 writes the scripts) heard the opening on F1; Down to Display mode and Vertical sync; Return to "Off" and "On"; Tab and Shift+Tab; Up to the tab; Right to "Audio & interface, tab, selected, 2 of 5..."; Master volume's Left and Right ("95%", "100%") and Return wrapping ("0%", "10%"); Controls' keyboard lines read with Up from Return to game ("IJKL C-stick", "Enter Start"); Online's field, connect code, typing ("A", "A#", Backspace to "blank") and Input delay; Escape to "Back to game"; gamepad back to "PC settings. Online, tab, selected, 5 of 5..."; leftshoulder to Controls; gamepad b to "Back to game"; and Down then got "VS. Mode. Multiplayer battles!". No "Saved" was spoken.
- **Checked.** Build, `python tools/check_style.py`, unit tests (`launcher_data` failing as known; `launcher_speech` and `port_menu_speech` passing), the menus level (19 of 19, the launcher's two among them). The grep inventory finds both new calls in `launcher.cpp`.
- **Docs.** `docs/a11y/rmlui-screens.md` (the port menu beside the launcher: the shared reader, one plugin a page, the one-context note, a section "The port menu" on its hooks, the frozen game, opening, closing and the status line rule, the test), ADR-0002's amendment, the `verify` skill (the unit test, how a run shows the menu now, "Out of reach"), CLAUDE.md's "Verification". The "Accessibility status" line is issue 06's.
- **Findings for the maintainer and issue 06.**
  - Escape in an Online field does nothing in the port menu either: the field keeps it, so the menu closes from there only after moving off the field, or with F1, B, Start or Back (base port behaviour, as in the launcher).
  - The update check's result is not written until the page refreshes: Check now writes "Checking for updates..." to the status line and the result row, and `PortMenu::refresh` writes the result only at the next saved change or opening, so a player hears it then (base port behaviour). A script checking it needs a change after Check now, and the network, so issue 06 may leave it out.
  - "Backend set to ... (restart required)." never shows: `apply("backend")` writes it, then falls through to `saved()`, which writes "Saved." over it in the same frame. The new value is still heard on the button. A base port bug worth reporting.
  - Changing the Controller row's port says only the new port ("Port 2"); the controller's name is the row's help text, heard on focus. The remapping feature is where to say it with the port.
