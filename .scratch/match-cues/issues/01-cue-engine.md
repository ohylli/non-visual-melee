# 01 The cue engine

Status: ready-for-agent
Type: task

The engine every cue plays through, before any cue exists. Read the spec's "The cue engine", "The volume chain" and "The log, the dump and the trace" first, and ADR-0005. The throwaway prototype on the local branch `prototype/audio-cues` (`cue_prototype.cpp`, `a11y.cmake`) shows the SDL stream, the miniaudio download and the threading; reuse what fits, keep none of its demo.

## Machinery

- **miniaudio 0.11.25**, downloaded at configure with its pinned SHA256 as the prototype does, compiled once in its own small library target in `a11y.cmake`. Its compile definitions (`MA_NO_DEVICE_IO`, `MA_NO_ENCODING`) are the target's public usage requirements, so every fork file that includes the header gets the same struct layout; the prototype set them file by file. Its own warnings off.
- **The engine module**: opens a second SDL audio stream on the default playback device (32-bit float stereo, 48 kHz) whose callback renders the cue mix. The render step is a plain function over a buffer, so tests drive it with no device.
- **Voices**: created with a mono sound buffer and a mode, looped or pulsed. Per-tick targets (pan, pitch, gain, interval) written by the game thread; the audio thread glides toward them over a few milliseconds. Start and stop through the fork's own short gain ramp. Pulses retriggered on the audio thread, each one interval after the last at the interval current then. Constant-power pan; pitch by playback rate.
- **Sound buffers**: a type the generators of later issues fill, checked against the fastest interval a pulsed voice allows: a pulse that does not fit is refused at voice creation with a log line.
- **The cue mix**: fixed headroom and a soft limit at the output.
- **Volume chain**: master × cue volume × per-cue volume × per-cue boost × voice gain. Cue volume and per-cue values are plain engine inputs here, set to 1; issue 02 feeds them from cue settings.
- **Master volume hook**: `pc_a11y_master_volume(float)`, one line in `pc_audio_set_volume` (`src/pc/audio.c`), declared in `a11y_hooks.h`. It stores the value even before the engine opens.
- **Lifecycle**: opened in `pc_a11y_init`, closed in `pc_a11y_shutdown`; all voices stop with a ramp on `pc_a11y_scene_entered`. Failure to open logs why, speaks "Audio cues could not start." and leaves the game running without cues.
- **Switch**: `MELEE_A11Y_CUES`, in `config_from_environment` beside steering: on when `MELEE_A11Y` is on, and `MELEE_A11Y_CUES=1` turns it on with speech off. Off means the engine never opens.
- **Logging**: an `[a11y] cue engine` line at open with the SDL driver, device rate and buffer size, as the prototype logs; each voice start and stop as `[a11y] cue <name> start|stop` with its targets.
- **Dump and trace**: `MELEE_A11Y_CUE_DUMP=<file>` writes the mix as raw f32 stereo 48 kHz from the audio thread; `MELEE_A11Y_CUE_TRACE=<file>` writes every voice's targets each tick, as a text line per voice.
- **Analysis script** under `tools/a11y/`: reads a dump and reports, per window, level, left-right balance, pulse onsets and their intervals, and dominant pitch.

## Done when

- `cmake --build build` passes, with the "Rollback sections" line.
- Unit tests render the engine into a buffer and cover: constant-power pan keeps loudness across pan; starts and stops have no sample-to-sample jump above a threshold; a pulsed voice's onsets are its interval apart and an interval change applies from the next pulse; the volume chain multiplies; five voices at full gain stay under full scale; a pulse longer than its fastest interval is refused.
- A test writes a dump of a known pulse train, and the analysis script reports its intervals and balance correctly.
- A bounded run (`MELEE_A11Y_CUES=1`, dummy audio driver) logs the engine's open line and shuts down cleanly.
- `python tools/check_style.py` passes; the footprint grep shows the new hook.
- The `verify` skill gains `MELEE_A11Y_CUES`, the dump, the trace, the script and the expected open line.
- `docs/a11y/cues.md` opens with the engine's mental model (two streams, voices with targets, pulses on the audio clock, the volume chain) and how to check a dump.
