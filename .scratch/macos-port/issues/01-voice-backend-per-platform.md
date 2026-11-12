# 01 The voice backend chosen per platform

Status: ready-for-agent
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
