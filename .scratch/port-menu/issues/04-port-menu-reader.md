# 04 The port menu reader

Status: ready-for-agent
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
