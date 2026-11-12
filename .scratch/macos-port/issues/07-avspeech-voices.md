# 07 The voices on a Mac

Status: ready-for-agent
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
