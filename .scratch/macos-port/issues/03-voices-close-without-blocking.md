# 03 The voices close without blocking the main thread

Status: resolved (2026-11-12)
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

## Comments

### 2026-11-12, implementation (agent)

Implemented; the game thread never waits for the voices' worker without giving the bridge its turns.

- **The turn.** `VoiceBridge::turn()` (`screen_reader_bridge.hpp`), pure virtual, the one call the game thread makes on the bridge: whatever the worker's calls wait for the main thread to do, done now, touching no instance. `NoVoiceBridge` does nothing in it. `PrismVoiceBridge::turn` calls its row's `turn`, a new function-pointer member of `PrismVoiceBackend`, `nullptr` in the OneCore row, so issue 07 only names the Mac's function in the AVSpeech row.
- **Waiting in turns.** `Voices::wait_in_turns` (`voices.cpp`) takes the lock and a condition: while it does not hold, it unlocks, gives the bridge its turn, relocks and waits on `m_idle` for at most `kTurnStep`. `~Voices` sets quit and waits so for `m_finished`, which the worker sets under the lock after `close` returns, then joins. A line still rendering finishes, as before, and the lines waiting are never rendered. `wait_for_worker`, the tests' only other wait, goes through the same function, and its comment says every wait of the game thread for the worker must. No other code on the game thread waits for the worker.
- **The fake.** `FakeVoiceBridge` with `VoiceCalls::needs_turns` set makes every call, not only `close` and `render`, wait for a turn given from another thread, as each AVSpeech call waits for a Mac's main thread; a wait longer than `kTurnTimeout` counts a timeout and goes on, so a test without the turns fails on its assert rather than hanging. A turn from the worker's own thread is counted too. The fake's `turn` touches only the turn's own counts.
- **Tests** (`test_voices.cpp`): `a_bridge_that_needs_turns_speaks` (the voices open, render, play and quit with such a bridge), `quitting_gives_the_bridge_turns_while_idle` (the close's turn given at quit) and `quitting_gives_the_bridge_turns_while_a_line_renders` (the worker held inside a render, waiting for its turn, as the voices quit: the render and then the close both get theirs). The last uses empty defaults so the line's first call is its render. With the destructor's bare join put back, the suite fails on a timeout after about 2 s; with the seam it passes in a third of a second, five runs in a row.
- **Verified.** Build, `check_style.py` and the unit tests pass (`launcher_data`, the known base port failure, alone failing). A `title` run of 400 frames with `MELEE_A11Y_SCREEN_READER=0 MELEE_A11Y_VOICE_PREVIEW=1` logs both voices' `plays` lines and `speech shutdown` and exits with 0. A `drive.py --muted` run with the preview, quit 120 ms after `voice yours plays: "Your voice"` while it still sounded, exits with 0. `run_level.py launcher_voices port_menu_voices`: 2 of 2 pass.
- **Docs.** `docs/a11y/speech.md`, "Shutting down", says why quit waits in turns; the `verify` skill's `voices` suite lists the new tests.

For the issues after this one:

- **07** fills the AVSpeech row's `turn` with a function that runs what Prism's AVSpeech backend has `dispatch_sync`ed to the main queue, without blocking, and returns: running the main run loop once (`CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true)`) should drain the main dispatch queue, as SDL's event pump does each frame. It must not block for long, since the game thread calls it every `kTurnStep` (5 ms) until the worker ends, and it must not touch the instances. `screen_reader_bridge.cpp` is kept free of platform headers, so the function may need a file of its own, declared to the bridge, built only on a Mac. Each of the worker's calls at quit may wait up to one step for its turn: a render with several hops to the main thread adds that many steps to quitting.
- **08** can check that quitting the game, with Cmd-Q and from the port menu's Quit game, while a voice speaks and while none does, ends the game at once.
