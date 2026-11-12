# 02 A voice saved by name and language

Status: resolved (2026-11-12)
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

## Comments

### 2026-11-12, implementation (agent)

Implemented; a voice is saved and matched by its display name and language together.

- **The saved form** is the proposal: the display name with the language tag after it in brackets, `Microsoft Zira (en-US)` (`voice_text` and `parse_voice_text` in `voices.cpp`). It is read from its end: a bracketed language tag there is the language, the rest the name, so `Eddy (English (UK)) (en-GB)` keeps the name's own brackets. A tag is two or three letters, then any parts of letters and digits after a hyphen or underscore (`en-US`, `fi`, `zh-Hans-CN`), so a name ending in brackets of its own, as macOS's `Ava (Enhanced)`, is not taken for name and language. Recorded in `docs/a11y/speech.md`, "Choosing a voice".
- **Text with no tag** is a name, or part of one, of any language, matched as before. So hand edits stay short (`Zira`), and the old lines still work. The one gap: a name written without its tag that ends in a bracketed two or three letters would be read as name and language. The rows always write the tag, so only a hand edit can hit it.
- **Matching** (`match_voice`, `find_voice`) works within the text's language, compared ignoring case: the exact name first, then the first voice whose name holds it. A language not installed, or no voice of the name in it, falls back to the backend's default voice. The worker and the game thread still share it.
- **The defaults.** `VoiceDefault` gained `language`, and OneCore's row is `{"Zira", "en-US"}, {"Mark", "en-US"}`, written to a fresh file as `Zira (en-US)` and `Mark (en-US)` and still matched by part of a name. An empty default name is the backend's own default voice whatever the language, saved empty as before (issue 05's AVSpeech row). Its language is ignored.
- **The rows** are unchanged to the ear. Each option's text is now the saved form and its label the name, with the language after it when the installed voices span more than one language (`voice_options`).
- **"Not installed"** says each voice as a row shows it. The missing one gets its language when the installed voices, counting it, span more than one; the voice in use is said as its row shows it. Example: "Voice Eddy, French (France) not installed, using Microsoft George, English (United Kingdom)." On a machine with a second language installed, as the maintainer's with its Finnish voice, the old "Voice Hazel not installed, using Microsoft George." now ends "..., English (United Kingdom)." Both missing still share one sentence, which gets long when both carry a language ("Voices Zira, English (United States) and Mark, English (United States) not installed, using ..."). Only a machine without the defaults' voices hears that, so it was left alone.
- **Tests.** `voices`: the saved form written and read back, brackets in names included; names repeating across languages each found by their text; part of a name within the language; a language not installed falling back; the worker setting the instance to the right voice. `voice_settings`: the options' texts and labels; voices of one name told apart, saved, read back and stepped; a name with brackets saved with its tag; a language not installed falling back with the "not installed" line; the defaults saved with their language; one-language and several-language wording of that line.
- **Drive scripts.** `launcher_voices` and `port_menu_voices` now expect `saved your_voice.voice = <name> (<tag>)` and check the file likewise. `launcher_a11y` and `port_menu_a11y` do not step the Voice rows and are unchanged. The `verify` skill and ADR-0008 follow.
- **Verified.** Build, `check_style.py` and the unit tests pass (`launcher_data`, the known base port failure, alone failing). A `title` run of 400 frames had `MELEE_A11Y_SCREEN_READER=0`, `MELEE_A11Y_VOICE_PREVIEW=1` and a settings file holding `your_voice.voice = Eddy (fr-FR)` and `opponents_voice.voice = Microsoft Mark (en-US)`. It logged `voices: OneCore, 7 installed, default Microsoft George`, `voice yours speaks with Microsoft George`, `voice opponents speaks with Microsoft Mark`, both samples' `plays` lines, and after the title screen `speak queue (muted): "Voice Eddy, French (France) not installed, using Microsoft George, English (United Kingdom)."`. The file kept both lines. `run_level.py launcher_voices port_menu_voices launcher_a11y port_menu_a11y`: 4 of 4 pass. On fresh settings the voices still speak with Zira and Mark, and steps save `Microsoft Heidi (fi-FI)`, `Microsoft Zira (en-US)` and the rest.

**The maintainer's `a11y.ini`.** Change `your_voice.voice = Microsoft Zira` to `your_voice.voice = Microsoft Zira (en-US)`, and `opponents_voice.voice = Microsoft Mark` to `opponents_voice.voice = Microsoft Mark (en-US)`. The old lines still find the same voices (a name without a tag matches in any language), so nothing breaks until then. A step on the Voice row writes the new form too.

For the issues after this one:

- **05**: the AVSpeech row's empty defaults need nothing more. `{""}` leaves the language empty, the voice is the backend's default and is saved empty.
- **07**: the Mac defaults should carry their language (`{"Samantha", "en-US"}` or whichever is chosen), so a name that repeats across languages finds the right one. Check the friend's Mac for voice names that end in a bracketed two or three letters with no language after them; none are known.
- **08**: the play test can check a repeated name on a Mac (an Eloquence voice such as Eddy in two languages): step to each and back, and see that each reads back after a restart.
