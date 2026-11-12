# 03 The voices close without blocking the main thread

Status: ready-for-agent
Type: task

Spec: `.scratch/macos-port/spec.md`, "The voices on a Mac" ("Every call runs on the main thread" and the one after it). Read it first, then `docs/a11y/speech.md` ("Voices", "Shutting down").

## Why

On a Mac, Prism's AVSpeech backend runs every call on the main thread and the calling thread waits for it. At quit, `Voices::~Voices` (run from the game's atexit handler, where `pc_a11y_shutdown` runs, on the main thread) joins the worker. The worker then frees its instances in `PrismVoiceBridge::close`, which on a Mac asks the main thread for work; or it is still rendering a line, which does the same. The main thread waits in `join`, the worker waits for the main thread: the game hangs on quit. Windows never hits this, but the seam can be built and tested here, so the Mac only fills in one function (issue 07).

## What to build

- **A turn for the bridge.** `VoiceBridge` gains a call the game thread makes while it waits for the worker, in which the bridge does whatever the worker has asked of the main thread. It must be safe to call while the worker uses the instances, so it touches none of them. Prism's bridge on Windows does nothing in it; the silent bridge too.
- **Quit waits in turns.** `Voices::~Voices` sets quit, then waits for the worker to finish in short steps (a done flag and a condition variable's timed wait, since `std::thread` has no timed join), giving the bridge its turn between steps; it joins only once the worker has finished. A line still rendering finishes or is dropped as today.
- **Nothing else waits on the worker from the game thread.** `Voices::wait_for_worker` is called by tests alone today; if anything else on the game thread ever waits for the worker, it waits the same way. Say so in a comment at the wait.

## Done when

- The voices' unit tests have a fake bridge whose `close` and `render` finish only once the bridge has had a turn on another thread, with a timeout that fails the test rather than hanging it. Without the seam the test fails; with it, it passes, quitting both while idle and while a line renders.
- Quitting on Windows is unchanged: a bounded run that opens the voices exits cleanly, and a run quit while a preview plays does too.
- `cmake --build build`, the unit tests and `python tools/check_style.py` pass; verify at the quick level.
- `docs/a11y/speech.md`, "Shutting down", says why quit waits in turns.
