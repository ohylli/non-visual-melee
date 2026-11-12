# 02 A voice saved by name and language

Status: ready-for-agent
Type: task
Blocked by: 01

Spec: `.scratch/macos-port/spec.md`, "The voices on a Mac" ("Names repeat"). Read it first, then `docs/a11y/speech.md` ("Voices", the rows and "A voice not installed").

## Why

A voice is saved by its display name alone, and `find_voice` takes the first voice of that name, else the first whose name holds the text. On Windows each OneCore voice has a name of its own. On a Mac some names repeat across languages, and since the list is sorted by language a saved "Eddy" could find the German Eddy. Prism gives each voice a name and a language and no stable id, so the two together are the voice.

## What to build

- **The saved text holds both**, readable and hand-editable by a screen reader user. A proposal: the name with the language tag after it in brackets, `Microsoft Zira (en-US)`, parsed from the end so a name with brackets of its own (some Mac voices may have them) still reads right. Choose another form if it reads better; record the choice in `docs/a11y/speech.md`.
- **Matching** takes name and language together: the exact name first, then the first voice whose name holds the text, both within the saved language. A missing voice falls back as now, to the backend's default voice.
- **The defaults** from issue 01 gain their language: `Zira` and `Mark` in `en-US` on Windows, still matched by part of a name.
- **What the rows show is unchanged**: the name, with the language's name after it when the installed voices span more than one language (`voice_options`, `voice_names.cpp`). Only the saved text changes.
- **"Not installed" announcements** say the missing voice as a row would show it.

No code reads a line written before this change: the only such file is the maintainer's own `a11y.ini`. Say in the issue's comment, when done, what the maintainer's line should be changed to.

## Done when

- The voices' and voice settings' unit tests cover: two voices of one name in different languages, each found by its saved text; a saved name whose language is not installed, falling back; the defaults found by part of a name in their language; the text parsed with brackets in the name.
- On Windows nothing heard changes: same voices chosen, same rows.
- `cmake --build build`, the unit tests and `python tools/check_style.py` pass; verify at the quick level and with the launcher and port menu scripts that step the Voice rows.
- `docs/a11y/speech.md` describes the saved form and the matching.
