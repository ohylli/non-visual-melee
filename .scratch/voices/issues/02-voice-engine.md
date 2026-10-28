# 02 The voices: rendering, playing and queueing

Status: ready-for-agent
Type: task

Spec: `.scratch/voices/spec.md`. Read it first, with ADR-0008, ADR-0005, `docs/a11y/speech.md` and `docs/a11y/cues.md`. Prior art: grimdark's `src/voice.cpp` and `src/tts_onecore.cpp` (`C:\Users\ottoh\koodi\grimdark`).

## What to build

- **Prism v0.18.2 first.** Answer the spec's open questions 1 and 2 before building on them: render a line with `prism_backend_speak_to_memory` on a OneCore instance from a worker thread, with a rate set, and note the apartment it needed. Record the answers in `docs/a11y/speech.md`.
- **The bridge.** `screen_reader_bridge` gains the voices' side: create a OneCore instance, list its voices (display name and language), set voice and rate, render a line to samples. It stays the only file that touches Prism. The fake renders known samples.
- **The worker.** One thread owns both instances, created once speech is first on and kept for the run. It renders, evens each line out to a fixed peak (a tuning value), converts it to mono at the cue engine's rate, and hands it to the cue engine.
- **The cue engine's new voice.** One per speaking voice: plays a buffer handed to it at run time, once, with the start and stop ramps, centred. Its chain is master volume × the voice's volume, not the cue volume or any cue switch.
- **Speech.** An announce call for a voice, with the line's subject. The queueing rules: a playing line about the same subject is cut with the stop ramp and the new one plays at once; a waiting one is replaced in place; otherwise the line queues. The two voices are independent of each other and of the screen reader.
- **Switches and the port menu.** Under Speech and Accessibility: off, voice lines are logged `(off)`. Turning either off, or opening the port menu, lets the playing line finish and drops the waiting ones. `MELEE_A11Y_SCREEN_READER=0`: decide open question 5 and record it.
- **Defaults.** Your voice the first voice whose name contains "Zira", the opponents' voice "Mark", else the Windows default voice. The rows, the fallbacks' announcements and saving the choice are issue 03's.
- **A developer switch** in the manner of `MELEE_A11Y_CUE_PREVIEW`, environment only, that has each voice say a line from the first frame, so this issue can be heard and checked through the cue dump before the rows exist.
- **The speech log.** Every voice line names its voice, with the `(off)` and muted marks; a line cut off or dropped is logged.
- **Measure** open question 3 (render time, short and long lines) and decide on the cache.

## Done when

- The voices' unit test (fake bridge and clock) covers the queueing rules, the two voices' independence, the switches and the port menu, the evening out and the speech log lines.
- The cue engine's test covers the new voice: played once, ramps at both ends, cut with a ramp, master volume and voice volume applied and cue volume not.
- A bounded run with the developer switch and `MELEE_A11Y_CUE_DUMP` shows both voices' lines in the log and their audio in the dump (`tools/a11y/cue_dump.py`).
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.
- The `verify` skill names the developer switch and its expected lines, as CLAUDE.md asks of a new switch.
- `docs/a11y/speech.md` has its "Voices" section started; `docs/a11y/cues.md` names the new voice and its chain.
