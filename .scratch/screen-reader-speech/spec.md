# Speech: the screen reader integration

Status: agreed 2026-09-23 (grilling session with the maintainer)

## Goal

The first accessibility feature: a fork-owned **speech** subsystem that every later feature (native menu narration, battle announcements, port menu, launcher) hands announcements to. It owns the Prism screen reader library, the speech log line and the runtime accessibility switch. The deliverable is the abstraction plus one proof-of-life announcement at startup.

Vocabulary is the glossary in `CONTEXT.md`: announcement, speech log, speech, screen reader bridge, hook.

## Decisions

### Scope

- The speech subsystem, its build wiring, one proof-of-life announcement at startup, a unit test, an ADR and a primer.
- No consumer beyond the proof-of-life announcement. Native menu narration is its own feature with its own grilling.
- The proof-of-life announcement is exactly `Non-Visual Melee ready`, spoken once at init, interrupting. No backend name in it.

### The announcement API

- An announcement is text plus a mode: **interrupt** (cut off current speech) or **queue** (speak after what is playing). Nothing else for now: no silence call, no priority beyond the two modes, no source tag. Consumers that need more will ask for it in their own grilling.
- The subsystem always speaks what it is told. No duplicate suppression: consumers that poll carry their own "last announced" state. Dedup inside the subsystem would make the speech log lie about what the hooks did.
- The subsystem remembers the last announcement (text and mode). A "repeat last announcement" key binding is a later feature; input handling stays out of this one.
- Text is UTF-8. Prism's `output` call is used, which reaches both speech and braille on screen reader backends; the one-text-per-announcement rule keeps the braille line from being overwritten mid-item.

### Backend choice

- Prism picks the backend through its `create_best` call (not `acquire_best`: Prism's docs say a program that has no reason to share backend state should create its own). Screen readers rank first; when none is running Prism falls back to OneCore or SAPI on its own. The fork accepts that: a player without NVDA still hears the game, and sighted helpers hear something.
- No re-acquire logic when NVDA restarts. Starship has none and the maintainer observed speech resuming by itself after an NVDA restart (the NVDA controller client makes a fresh remote call per speak). Prism's newer docs say the NVDA backend never reconnects; whether that applies to the pinned version is a play-test question (issue 06). Re-acquire on failure becomes a follow-up only with evidence.

### Threading

- Main-thread only. No queue, no per-frame tick. Prism backends are not thread-safe, the port's logger is not thread-safe, and every planned consumer runs on the game thread. Prism's speak returns as soon as speech starts, so it never blocks a frame.
- The subsystem records the thread it was initialised on and treats a call from another thread as a bug: logged as `[a11y] speech called off the game thread, dropped: "text"` and not spoken. A queue can be added inside the subsystem later without touching consumers.

### Runtime switch

- Accessibility is compiled in always and switched at runtime, default on. For this cut the switch is the environment variable `MELEE_A11Y`; the value `0` turns speech off. The speech log has its own developer switch `MELEE_A11Y_LOG`, default on, `0` turns it off.
- The subsystem reads its configuration through one small struct filled by one function, so swapping the source for a fork-owned settings file later touches that function only. The port's own `launcher.cfg` is not used: it drops unknown keys on every save and each setting there costs four edits in base port files.
- Switched off means: no Prism init, no speech, but the speech log still records the announcement with a `(off)` marker so a bounded run can be checked either way.

### Speech log

Written through the port's `pc_log_line`, one line per event, prefix `[a11y]`, fixed fields first, text last and quoted:

```
[a11y] speech backend: NVDA
[a11y] speech backend: none (silent)
[a11y] speech off (MELEE_A11Y=0)
[a11y] speak interrupt: "Non-Visual Melee ready"
[a11y] speak queue: "Stock lost"
[a11y] speak interrupt (off): "Non-Visual Melee ready"
[a11y] speak failed (<prism error>): "text"
[a11y] speech called off the game thread, dropped: "text"
[a11y] speech shutdown
```

Note for CLAUDE.md: the log file lands in the working directory (`melee-pc.log` is a relative path), not beside `melee.exe`.

### Prism in the build

- Prism cannot be built from source with the fork's GCC toolchain: its Windows build requires Microsoft's MIDL tool for the NVDA stubs, and its own CMake warns that under MinGW "Prism will malfunction" because delay loading is unsupported. Its CI builds Windows with MSVC only. See ADR-0001.
- `src/pc/a11y/a11y.cmake` downloads the pinned release zip (`prism-windows-x64.zip` of v0.18.2, sha256-checked) at configure time into the build directory, uses its header and import library, and copies `prism.dll` beside `melee.exe` like the other runtime DLLs. Nothing binary in git. The pin is one line to bump.
- Build-time linking against the import library, not runtime loading. A missing `prism.dll` then stops `melee.exe` with the standard Windows "prism.dll was not found" dialog, which NVDA reads. A silent start would be indistinguishable from "speech is off" for a blind player. Consequence: `prism.dll` is a required file for everyone, like SDL's DLL.
- Prism is MPL-2.0, compatible with the fork's GPL-3.0-or-later.
- Non-Windows builds get a stub screen reader bridge (no Prism download, no link), so the port keeps building elsewhere. The fork only supports Windows for its own work.

### Names and layout

All fork code under `src/pc/a11y/`, C++20 internals, namespace `a11y`.

| File | Role |
| --- | --- |
| `a11y.cmake` | Prism download, fork sources, test target. Included from the root `CMakeLists.txt` with one line. |
| `a11y_hooks.h` | The single C-compatible header of every `pc_a11y_*` hook function. |
| `hooks.cpp` | `extern "C"` implementations of the hooks, delegating to the subsystem. |
| `speech.hpp`, `speech.cpp` | Speech: announce, switch, speech log, last announcement, thread check. Talks to the bridge through a small interface so a fake can replace it. |
| `screen_reader_bridge.hpp`, `screen_reader_bridge.cpp` | Screen reader bridge: the only file that includes `prism.h`. Real body when `a11y.cmake` defines `A11Y_HAVE_PRISM` (Windows today), stub otherwise. Gated on that macro, not on `_WIN32`, so a macOS or Linux port is a cmake branch only. |
| `test_speech.cpp` | Unit test with a fake bridge. |

Hooks in this feature, both one line in `src/pc/main.c`:

- `pc_a11y_init(void)` right after `pc_launcher_configure(&config)`, so it runs before the launcher window opens and on the command-line-disc path alike. Speaks the proof-of-life announcement.
- `pc_a11y_shutdown(void)` at the top of `pc_shutdown_once()`, which also runs when the window is closed.

### Tests

- A unit test executable under the base port's ctest setup (label `melee`), built the way `tools/test_*.cpp` are: `EXCLUDE_FROM_ALL`, hooked to the `unit_tests` target. It links speech with a fake bridge that records calls, and defines its own `pc_log_line` that captures lines, since the real one lives in `main.c`.
- Covered: interrupt and queue reach the bridge with the right flag, the log line format, off-switch behaviour (nothing reaches the bridge, log carries the `(off)` marker), last announcement, off-thread call dropped and logged.
- The agent's own check after a change: a bounded `title` run logs `[a11y] speech backend: ...` and `[a11y] speak interrupt: "Non-Visual Melee ready"` and exits cleanly.

### Docs

- ADR-0001: prebuilt Prism DLL instead of building from source.
- Primer `docs/a11y/speech.md`: mental model first (what an announcement is, where it goes, what the log shows), then the switch, the log format, the Prism pin and how to bump it, the threading rule.
- CLAUDE.md: one line under "Accessibility status" once the feature lands; fix the log-file location sentence.

## Play-test (maintainer, by ear)

1. Launch normally with NVDA running: hear "Non-Visual Melee ready" before the launcher appears. Log shows `speech backend: NVDA`.
2. Launch with NVDA closed: hear the same through a Windows voice. Log shows the fallback backend's name.
3. With the game running, close NVDA, then restart it. Does speech resume by itself? (Only the proof-of-life line exists yet, so this test waits for the first real consumer, or a temporary key; note it for the menu narration feature.)
4. `MELEE_A11Y=0`: silence, log shows `speech off` and the `(off)` line.
5. Delete `prism.dll` from the build directory and launch: NVDA reads the Windows "not found" dialog, the game does not start. Restore the file.

## Out of scope, noted for later

- Silence call, priorities, source tags: when a consumer needs them.
- Fork-owned settings storage: when the port menu feature is specced.
- Repeat-last key binding: with input handling.
- Re-acquire on NVDA restart: only with play-test evidence.
- Off-thread announcements (netplay timer thread): a queue inside the subsystem, if ever needed.
- macOS: a cmake branch for `prism-macos-universal.zip` plus a VoiceOver play-test on a borrowed Mac. Bundling and ad-hoc signing are already handled by the base port's packaging script. Plan in `.scratch/macos-port/spec.md`.
