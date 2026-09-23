# Speech

## Mental model

An **announcement** is one piece of text plus a mode: **interrupt** (cut off whatever the screen reader is saying) or **queue** (say it after). A feature that has something to tell the player builds one announcement and hands it to **speech**, the fork's subsystem in `src/pc/a11y/speech.cpp`.

Speech does three things with it: it writes a line to the **speech log**, it remembers it as the last announcement, and it passes it to the **screen reader bridge** (`screen_reader_bridge.cpp`). The bridge is the only fork file that knows about Prism, the screen reader library. Prism passes the text to whatever screen reader is running (NVDA, JAWS and others), which speaks it and shows it on a braille display. If none is running, Prism uses a Windows voice (OneCore or SAPI) instead.

```
feature --announce--> speech --log line--> melee-pc.log
                         |
                         +--> screen reader bridge --> Prism --> NVDA / JAWS / Windows voice
```

Speech speaks exactly what it is given, every time. It does not drop repeats: a feature that checks game state every frame keeps its own "last said" state. That way the speech log shows exactly what each hook asked for.

Today the only announcement is the proof of life: "Non-Visual Melee ready", spoken once at startup, before the launcher window opens.

## Hooks

Base port files call the fork through **hooks**, one-line calls to `pc_a11y_*` functions declared in `src/pc/a11y/a11y_hooks.h` and implemented in `hooks.cpp`. Speech has two, both in `src/pc/main.c`:

- `pc_a11y_init()` right after `pc_launcher_configure()`: starts speech and speaks the proof of life.
- `pc_a11y_shutdown()` at the start of `pc_shutdown_once()`, which also runs when the window is closed.

## Switches

Both are environment variables for now. `0` turns them off; unset or any other value means on.

| Variable | Default | Off means |
| --- | --- | --- |
| `MELEE_A11Y` | on | Prism is never started and nothing is spoken. The speech log still records each announcement, marked `(off)`. |
| `MELEE_A11Y_LOG` | on | No `[a11y]` lines in the log. Speech is unaffected. |

`config_from_environment()` in `speech.cpp` is the only code that reads them. When the fork gets its own settings storage (planned alongside the port menu), only that function changes.

## The speech log

Every event is one line in the game's log, through the port's `pc_log_line`, prefixed `[a11y]`. Fixed fields come first; the spoken text comes last, in quotes. On Windows the log is `melee-pc.log` in the working directory (the folder the game was started from), or the file named by `MELEE_LOG_FILE`. Each line starts with the milliseconds since the first log line.

```
[    0.000] [a11y] speech backend: NVDA
[    0.000] [a11y] speak interrupt: "Non-Visual Melee ready"
...
[ 7061.961] [a11y] speech shutdown
```

All the line shapes:

| Line | Meaning |
| --- | --- |
| `speech backend: <name>` | Prism picked this backend at startup. |
| `speech backend: none (silent)` | Prism found nothing to speak through. |
| `speech off (MELEE_A11Y=0)` | The accessibility switch is off. |
| `speak interrupt: "<text>"`, `speak queue: "<text>"` | An announcement went to the screen reader. |
| `speak interrupt (off): "<text>"` | An announcement was made while the switch is off; nothing was spoken. |
| `speak failed (<Prism error>): "<text>"` | Prism refused the text. |
| `speech called off the game thread, dropped: "<text>"` | A bug: see the threading rule. |
| `speech shutdown` | Speech stopped at exit. |

This log is how the fork is verified without hearing it: a bounded run (see CLAUDE.md, "Verification") ends with these lines in the log file, and after a play-test the log is read against what the tester heard.

## Threading rule

Speech runs on the game thread only, the thread that called `pc_a11y_init`. Prism's backends are not thread-safe and neither is the port's logger. Every feature planned so far runs on the game thread anyway. Prism's output call returns as soon as speech starts, so it never holds up a frame.

A call from any other thread is a bug. It is logged as `speech called off the game thread, dropped`, not spoken, and not remembered as the last announcement. If a feature on another thread ever needs to speak (the netplay timer thread is the likely one), a queue inside speech can hand announcements over to the game thread without changing any feature.

## Prism: the pinned prebuilt DLL

Prism is not built from source: the fork's GCC toolchain cannot build it on Windows. [ADR-0001](../adr/0001-prebuilt-prism-dll.md) records why. Instead, `src/pc/a11y/a11y.cmake` handles it at configure time:

1. Downloads `prism-windows-x64.zip` for the pinned release from GitHub, checking its SHA-256 hash, into `build/prism-<tag>/`. It keeps the header, the dynamic release build and the licence files. A later configure finds the extracted folder and skips the download, so it works offline.
2. Links `melee.exe` against Prism's import library (`prism.lib`, a small file that tells the linker which functions live in `prism.dll`).
3. Copies `prism.dll` beside `melee.exe` after each build.

`prism.dll` is a required file, like SDL's DLL. If it is missing, Windows refuses to start the game and shows a "prism.dll was not found" dialog, which a screen reader reads. That was chosen over loading Prism optionally, so a broken install is never mistaken for speech being switched off.

**Bumping Prism**: change `A11Y_PRISM_VERSION` (the release tag) and `A11Y_PRISM_SHA256` together at the top of `a11y.cmake`. Get the hash by downloading the new zip and running `sha256sum` on it. The folder name includes the tag, so the next configure downloads the new release. Check the new `prism.h` for API changes, then build, run the unit test and do a bounded run.

## Tests

`src/pc/a11y/test_speech.cpp` tests speech against a fake bridge that records every call. It runs with the base port's unit tests: `cmake --build build --target unit_tests` then `ctest --test-dir build -L melee` (test name `speech`).

## Other platforms

The fork supports Windows only, but porting speech to another platform should be a build change, not a code change. The bridge's real body is compiled when `a11y.cmake` defines `A11Y_HAVE_PRISM`, which it does wherever it set up Prism. Everywhere else the bridge compiles as a silent stub and the log still works. Prism publishes macOS and Linux zips under the same release tag with the same C API. A port is a new branch in `a11y.cmake` (zip name, hash, library to link, where to copy it) plus a play-test by ear. On macOS the base port's packaging script already bundles and ad-hoc signs every dylib the game links, which would include Prism's. The worked plan is `.scratch/macos-port/spec.md`.
