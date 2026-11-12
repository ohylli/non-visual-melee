# 01 The voice backend chosen per platform

Status: resolved (2026-11-12)
Type: task

Spec: `.scratch/macos-port/spec.md`, "The voices on a Mac". Read it first, then `docs/a11y/speech.md` ("Voices") and ADR-0008.

## What to build

The voices' Prism backend is hard-wired today: `PrismVoiceBridge::open` creates `PRISM_BACKEND_ONE_CORE`, and OneCore's name and voices are written into the code around it. Make the backend a per-platform choice, OneCore on Windows, so issue 07 only adds the Mac's values.

- **The choice in `a11y.cmake`**, beside the Prism asset: each platform branch names its voice backend (a compile definition), as it names the zip. Windows names OneCore. A platform with Prism but no voice backend named gets no voices, as a platform without Prism does now.
- **What a backend brings**, kept in `screen_reader_bridge.cpp`, the one file that knows Prism (CLAUDE.md, "Build and run (Windows)": platform specifics stay in the bridge): its Prism id, the name the log uses ("OneCore"), and the default voice of each voice ("Zira" for your voice, "Mark" for the opponents' today). The defaults are needed when the rows are registered, before the voices open, so they come from a function of the build, not of an open bridge. Issue 02 adds a language to each default; leave room for it.
- **Platform-neutral words.** Where the code names OneCore or Windows for what is really "the voices' backend": the log line `voices: OneCore, ...` takes the backend's name; the Voice row's description ("The Windows voice that says ...") names no platform; any announcement the player hears says nothing platform-specific. Comments that describe OneCore's own behaviour (its loudness, its apartment) stay, marked as OneCore's.
- **Docs.** ADR-0008 gains a line that the backend is per platform, OneCore on Windows, AVSpeech planned for macOS (`.scratch/macos-port/`). `docs/a11y/speech.md`, "Voices", says where the choice lives.

SAPI as a second Windows choice is out of this issue.

## Done when

- On Windows the voices behave exactly as before: same voices, same defaults, same log line apart from the backend's name coming from the table.
- The voices' unit tests pass, with a test that the defaults come from the build's backend.
- `cmake --build build`, the unit tests and `python tools/check_style.py` pass; verify at the quick level, plus a run that opens the voices (`MELEE_A11Y_VOICE_PREVIEW`) and shows the log line.
- `grep -rn "OneCore\|Windows" src/pc/a11y --include=*.cpp --include=*.hpp` finds them only in the bridge, in `netcheck.cpp`, and in comments about OneCore's own behaviour.

## Comments

### 2026-11-12, implementation (agent)

Implemented; the voices' backend is a per-platform choice, OneCore on Windows, and nothing outside the bridge names it.

- **The choice in `a11y.cmake`.** Each platform branch sets `A11Y_VOICE_BACKEND` to a Prism backend id, by Prism's own macro name (`PRISM_BACKEND_ONE_CORE` on Windows), beside its asset; inside the Prism block it becomes the compile definition `A11Y_VOICE_BACKEND=<id>`. An empty value names none.
- **The table**, `kVoiceBackends` in `screen_reader_bridge.cpp`, under `A11Y_HAVE_PRISM`: rows of `PrismVoiceBackend`, the Prism id plus a `VoiceBackend` (`screen_reader_bridge.hpp`: the log's name and `defaults`, one `VoiceDefault` per voice, in `Voice` order). The build's row is found at compile time, and a `static_assert` stops the build if `a11y.cmake` names a backend the table lacks. `voice_backend()` gives it, or an empty backend where Prism is missing or no backend is named; `make_voice_bridge()` then gives a bridge whose open fails with "no voice backend on this platform" (Prism, no backend) or "no Prism on this platform" (no Prism, as before). `PrismVoiceBridge` holds its row: the id it creates instances with and the name in its errors ("OneCore did not start: ...").
- **The defaults reach the code by injection.** `Voices` and `VoiceSettings` take a `const VoiceBackend&`; `hooks.cpp` passes `voice_backend()`, the tests their own (`kTestBackend` in `test_voice_bridge.hpp`, named "Fake voices", defaults Zira and Mark). So "the defaults come from the build's backend" is tested as: the voices and their rows take every default and the log's name from the backend given (`the_defaults_come_from_the_backend` in both `test_voices.cpp` and `test_voice_settings.cpp`, with a backend whose defaults are Heidi and empty). The tests do not link the bridge, so the real table itself is checked by the game run below.
- **An empty default works**, ready for issue 05: it means the backend's own default voice, is not "not installed", and is saved empty and reads back empty (tested).
- **Platform-neutral words.** The log line takes the backend's name; the Voice row's description is now "The voice that says ..." (the four drive scripts that check it, `launcher_a11y`, `launcher_voices`, `port_menu_a11y` and `port_menu_voices`, and the `verify` skill follow); "the Windows default voice" is "the backend's default voice" in comments and docs. No announcement the player hears named a platform otherwise. The grep now finds OneCore and Windows in the bridge, `netcheck.cpp`, OneCore's own loudness (`voices.hpp`) and its voices' languages (`voice_names.*`), and two comments about Windows' own file behaviour that have nothing to do with the voices (`accessibility_settings.cpp`, `value_file.cpp`: `getenv`'s code page, a rename refused while another program holds the file), left as they are.
- **Docs.** `docs/a11y/speech.md` has a new "The voice backend" section under "Voices" and its generic wording; ADR-0008 gains the per-platform line; CLAUDE.md's Voices status line names the table.
- **Verified.** Build, `check_style.py` and the unit tests (`launcher_data`, the known base port failure, alone failing); a `title` run of 400 frames with `MELEE_A11Y_SCREEN_READER=0 MELEE_A11Y_VOICE_PREVIEW=1` and a cue dump logs `voices: OneCore, 7 installed, default Microsoft George, ...`, `voice yours speaks with Microsoft Zira`, `voice opponents speaks with Microsoft Mark` and both samples' `plays` lines, and the dump shows two centred stretches a pause apart, peaking alike; `run_level.py menus`, 34 of 34.

For the issues after this one:

- **02** adds `std::string_view language;` to `VoiceDefault` and `"en-US"` to the OneCore row's two defaults; `Voices` and `VoiceSettings` read `backend.defaults[voice]` in one place each.
- **05** adds the Mac's row to `kVoiceBackends`, `{PRISM_BACKEND_AV_SPEECH, {"AVSpeech", {{{""}, {""}}}}}`, with a comment that both voices use Apple's default voice until issue 08 picks two, and `set(A11Y_VOICE_BACKEND PRISM_BACKEND_AV_SPEECH)` in its branch of `a11y.cmake`. Nothing else in the C++ changes.
- **07** adds its turn as a member of `PrismVoiceBackend` (a function pointer, null for OneCore), reached through `PrismVoiceBridge::m_backend`, and fills the Mac row's defaults.
