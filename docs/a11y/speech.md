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

With speech off, by its switch or Accessibility's (below), announcements are still logged, marked `(off)`, and the speech log goes on recording. A run can check a feature's announcements without talking over the maintainer's screen reader: `MELEE_A11Y=0` turns Accessibility off for the run. Prism is started the first time speech is on, so a run that never turns it on never starts Prism.

A feature that takes keys for speech leaves them alone with speech off (the launcher's line reading is one), so a run that checks such a feature needs speech on without the screen reader: `MELEE_A11Y_SCREEN_READER=0` keeps accessibility on, never starts Prism, and logs each announcement marked as muted.

The port's logger cuts each line at 512 bytes, so a very long announcement is truncated in the log. What gets spoken is not affected.

## The switches

Four switches are settings like any other, rows of the accessibility settings in `a11y.ini` (`docs/a11y/cues.md`, "Accessibility settings"), the first rows of the menus: Accessibility (`accessibility.on`), Speech (`speech.on`), Steering (`steering.on`) and Cues (`cues.on`, the cue settings' own). `src/pc/a11y/switches.cpp` works out what they leave on after every change to the settings, and the hooks hand it on. Every switch is live: no restart.

- **The hierarchy.** Accessibility is above Speech, Steering and Cues; Cues is above each cue's own switch. A higher switch off silences the ones below it and leaves their saved values as they are, so turning it back on restores what the player had.
- **What off does.** Speech off stops speech and leaves the cues and steering. Steering off ends a glide under way where the hand or cursor is, puts the stick back to rest at the next pad the hook publishes, and leaves the D-pad to the game and to quick chat. Cues off mutes the cue engine, which stays open and keeps its voices running underneath, so the cues come back where the match is. Accessibility off does all three: the readers and scene speech go on following the screens, so their state stays current, and only their speech goes unheard.
- **The last announcement.** Turning Speech or Accessibility off says "Speech off" or "Accessibility off" while speech is still on, then speech goes quiet. The screen reader got that text before anything else could be sent, and nothing sent after reaches it, so nothing cuts it off. Turning either back on says "Speech on" or "Accessibility on" first.
- **The way back into a page.** A reader learns that speech is back by watching `Speech::enabled()`: the RmlUi readers (`PageReader::follow_focus`) announce the focused row again, queued behind the switch's words, on their next frame, when the page already shows the new value; the main menu tree says its screen afresh on its next frame. The other native screens' readers say nothing until something changes.
- **Startup.** Speech is silent until the settings are read, so a player who saved Accessibility off hears nothing, not even the proof-of-life announcement, and Prism is never started. A problem found as the file was read is said after the proof-of-life.

### The environment overrides the file

`MELEE_A11Y` (Accessibility), `MELEE_A11Y_STEER` (Steering) and `MELEE_A11Y_CUES` (Cues), when set, hold their switch for the run whatever `a11y.ini` says: `0` off, anything else on, empty as if unset. The file keeps its own value: an override is never written, and a hand edit of that line waits under it. A change from a menu lifts the override and is saved as usual.

- `MELEE_A11Y=1` forces Accessibility on, the way back for a player who saved it off and starts the game silent; editing `a11y.ini` is the other.
- An override of Steering or Cues holds against Accessibility as well, so an agent run with Accessibility off (`MELEE_A11Y=0`) still steers with `MELEE_A11Y_STEER=1` and hears cues with `MELEE_A11Y_CUES=1`. Without an override they sit under Accessibility like any switch.
- The developer switches stay environment-only, never settings: `MELEE_A11Y_LOG`, `MELEE_A11Y_SCREEN_READER`, the cue dump and trace, the event pipe and the text dump.

## Threading rule

Speech runs on the game thread only, the thread that started it. Prism's backends are not thread-safe, and neither is the port's logger. Every feature planned so far runs on the game thread anyway.

A call from any other thread is a bug: speech logs it and drops it. If a feature on another thread ever needs to speak (the netplay timer thread is the likely one), a queue inside speech can hand announcements to the game thread without changing any feature.

Prism's output call is expected to hand the text over and return without waiting for speech to finish, so it should not hold up a frame. That is an assumption from Prism's API, not a measurement. If a hitch is ever noticed, time the Windows voice fallback (SAPI) first.

## Prism is a prebuilt, required DLL

The fork's GCC toolchain cannot build Prism, so the build downloads a pinned release and links its DLL; [ADR-0001](../adr/0001-prebuilt-prism-dll.md) records why, and why a missing `prism.dll` stops the game instead of silently turning speech off. Bumping the release is described where the pin lives, at the top of `src/pc/a11y/a11y.cmake`.

## Other platforms

Porting speech to another platform should be a build change, not a code change: Prism publishes macOS and Linux builds with the same C API, and where the build has not set Prism up, the bridge compiles as a silent stub while the log keeps working. The worked plan for macOS is `.scratch/macos-port/spec.md`.
