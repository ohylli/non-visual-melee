# Speech

## Mental model

An **announcement** is one piece of text plus a mode: **interrupt** (cut off whatever the screen reader is saying) or **queue** (say it after). A feature that has something to tell the player builds one announcement and hands it to **speech**, the fork's subsystem in `src/pc/a11y/speech.cpp`.

Speech writes the announcement to the **speech log** and passes it to the **screen reader bridge** (`screen_reader_bridge.cpp`). The bridge is the only fork file that knows about Prism, the screen reader library. Prism passes the text to whatever screen reader is running (NVDA, JAWS and others), which speaks it and shows it on a braille display. If none is running, Prism uses a Windows voice (OneCore or SAPI) instead.

```
feature --announce--> speech --log line--> melee-pc.log
                         |
                         +--> screen reader bridge --> Prism --> NVDA / JAWS / Windows voice
```

## Speech never filters

Speech says exactly what it is given, every time, and never drops a repeat. A feature that checks game state every frame keeps its own "last said" state. That keeps the speech log an exact record of what each hook asked for.

## The speech log

Every announcement, and every speech event worth knowing about (startup, the backend Prism picked, failures, shutdown), becomes one `[a11y]` line in the game's log. The log is how the fork is verified without hearing it: an agent's bounded run checks for the expected lines, and after a play-test the log is read against what the tester heard.

With the accessibility switch off (`MELEE_A11Y=0`), Prism is never started, but announcements are still logged, marked as not spoken. A run can check a feature's announcements without talking over the maintainer's screen reader.

The port's logger cuts each line at 512 bytes, so a very long announcement is truncated in the log. What gets spoken is not affected.

## Threading rule

Speech runs on the game thread only, the thread that started it. Prism's backends are not thread-safe, and neither is the port's logger. Every feature planned so far runs on the game thread anyway.

A call from any other thread is a bug: speech logs it and drops it. If a feature on another thread ever needs to speak (the netplay timer thread is the likely one), a queue inside speech can hand announcements to the game thread without changing any feature.

Prism's output call is expected to hand the text over and return without waiting for speech to finish, so it should not hold up a frame. That is an assumption from Prism's API, not a measurement. If a hitch is ever noticed, time the Windows voice fallback (SAPI) first.

## Prism is a prebuilt, required DLL

The fork's GCC toolchain cannot build Prism, so the build downloads a pinned release and links its DLL; [ADR-0001](../adr/0001-prebuilt-prism-dll.md) records why, and why a missing `prism.dll` stops the game instead of silently turning speech off. Bumping the release is described where the pin lives, at the top of `src/pc/a11y/a11y.cmake`.

## Other platforms

Porting speech to another platform should be a build change, not a code change: Prism publishes macOS and Linux builds with the same C API, and where the build has not set Prism up, the bridge compiles as a silent stub while the log keeps working. The worked plan for macOS is `.scratch/macos-port/spec.md`.
