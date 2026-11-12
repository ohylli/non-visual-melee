# 07 The voices on a Mac

Status: resolved (2026-11-12)
Type: task
Blocked by: 01, 02, 03, 05, 06

Spec: `.scratch/macos-port/spec.md`, "The voices on a Mac". Read it first, then Prism v0.18.2's `source/backends/avspeech.cpp` (`sync_on_main`, `wait_for_semaphore_pumping_main`, `initialize`, `speak_to_memory`), and issues 01 and 03's comments.

## What to build

The Mac half of issue 03 and the Mac values of issue 01. Written here, compiled by issue 06's CI, run on the friend's Mac in issue 08.

- **The bridge's turn on a Mac**: run the main thread's queue for a moment, so the worker's waiting `dispatch_sync` calls go through. `CFRunLoopRunInMode(kCFRunLoopDefaultMode, <a short time>, true)` from CoreFoundation's C API does it from C++, with no Objective-C; link the CoreFoundation framework in the Mac branch of `a11y.cmake`. Keep it inside `screen_reader_bridge.cpp`, gated on the Mac backend.
- **Anything else on a Mac that waits.** The voices open on the worker while the main thread runs the game, so opening needs no turn; check there is no other place the main thread waits for the worker (issue 03 left a comment at the one wait).
- **The default voices.** Both voices use Apple's default voice until the friend picks two by ear in issue 08; then set them, by name and language, in issue 01's table.
- **The comments in the bridge** about OneCore (the apartment, the context left to the process's end) say which backend they concern, and the Mac's own reasons are written beside them.

## Done when

- Issue 06's CI builds it.
- The friend's run in issue 08 quits without hanging, with the voices open and with a line playing.
- The default voices are set from issue 08's answer.

## Comments

### 2026-11-12, implementation (agent)

Implemented what can be done before the play test; the two items only a Mac can settle moved to issues 08 and 09 (below).

**The turn.** `run_main_queue` in a new Mac-only file, `src/pc/a11y/main_queue_mac.cpp`: on the main thread, `CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true)` (CoreFoundation's C API, no Objective-C); on any other thread nothing, since only the main run loop serves the main queue. `a11y.cmake`'s Mac branch alone builds it, links `-framework CoreFoundation` and defines `A11Y_MAIN_QUEUE_TURN`, under which `screen_reader_bridge.cpp` declares the function and puts it in the AVSpeech row's `turn` (`kMainQueueTurn`, `nullptr` elsewhere, so the row still compiles on Windows). The bridge keeps no platform header, as issue 03 asked, rather than the issue's "inside `screen_reader_bridge.cpp`".

**Why a time of 0.** Prism v0.18.2's `avspeech.cpp` sends work with `dispatch_sync(dispatch_get_main_queue(), ...)` (`sync_on_main`) whenever the caller is not the main thread, and the main queue is served by the main thread's run loop in its common modes, default among them. A pass with a time of 0 runs what is already queued and returns at once; the game thread's own wait between turns (`kTurnStep`) is the pause, so the turn never blocks.

**Where AVSpeech waits for the main thread**, read in the source: `initialize` (creating the synthesizer, the voice list, the format probe), `refresh_voices`, `speak_to_memory` (one hop to start the utterance, then a wait on a semaphore for the synthesizer's last buffer, which may come back through the main thread too), and the destructor (stopping both synthesizers), so `close` takes one hop per instance. `set_voice`, `set_rate`, `count_voices`, the voice names and `get_voice` touch only atomics and the backend's own lock. The worker makes every bridge call outside the voices' lock, and `prism.cpp` takes no lock of its own, so no other wait of the game thread for the worker exists: `~Voices` is the one, already in turns (issue 03).

**A finding for the defaults.** Prism's AVSpeech "default voice" (`refresh_voices`) is the first voice in `[AVSpeechSynthesisVoice speechVoices]` whose language equals `currentLanguageCode` exactly, else the first voice of all; it then names that voice on every utterance. So the empty defaults speak with that voice, not the one chosen in System Settings' Spoken Content, and which it is shows only on the Mac. Recorded in the bridge's row comment and `docs/a11y/speech.md`; issue 08's check 9 now asks for it.

**Comments and docs.** The bridge's comments on the worker's own context, `close` (the context left to the process's end), the render's callback and `turn` say which backend each concerns, with AVSpeech's reasons beside OneCore's. `docs/a11y/speech.md` has "The main thread's turn on a Mac" under "macOS", and "The voice backend" says what AVSpeech's default is. The `verify` skill's "Mac build" names `main_queue_mac.cpp` among what only CI compiles.

**Verified.** On Windows at the quick level: build (the "Rollback sections" line), `check_style.py`, the unit tests (`launcher_data`, the known base port failure, alone failing), a `title` run of 400 frames with `MELEE_A11Y_SCREEN_READER=0 MELEE_A11Y_VOICE_PREVIEW=1` (OneCore, Zira and Mark, both `plays` lines, `speech shutdown`, exit 0), and `run_level.py launcher_voices port_menu_voices`, 2 of 2. CI on `macos-ci`, https://github.com/ohylli/non-visual-melee/actions/runs/43232919371: all three build jobs pass, `main_queue_mac.cpp` compiled and `melee` linked with no new warning, both Mac zips as artifacts, the bundle's signatures valid, `melee --version` run from the zip, and the unit tests 38 of 38 on both Mac runners. The code has not run on a Mac.

**Moved, as agreed with the coordinator:**

- "The friend's run quits without hanging" is issue 08's check 12, now asking for both ways to quit (the port menu's Quit game and Cmd-Q), idle and while a line plays, and saying what a hang looks like in the log.
- "The default voices are set from 08's answer": issue 08's check 9 records the two voices by name and language, and issue 09's "What to build" now sets them in the AVSpeech row (with their docs), its "Done when" the build and checks for it.

For the play test (08): listen for how late a voice line comes after its cue in a match, since each hop to the main thread waits for SDL's next event pump (the log's `rendered in`/`after it was said` figures); for lines held through a scene loading; and that quitting is at once, not after a pause.
