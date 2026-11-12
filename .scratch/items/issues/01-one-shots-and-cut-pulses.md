# 01 The cue engine: one-shot voices, pulses cut with a fade, more voices

Status: ready-for-agent
Type: task
Blocked by: none

Spec: `.scratch/items/spec.md` ("The cue engine"). Read it first, then `docs/a11y/cues.md` ("Mental model", "The engine's rules", "The once voice") and ADR-0005.

## What to build

- **A one-shot voice.** A new voice mode for cues: created with a sound, it plays that sound once from the start each time it is triggered, with pan, pitch and gain targets, through its cue's level, so Accessibility, Cues, the cue volume and the cue's own switch, volume and boost reach it as they reach every cue voice. A trigger while it still plays restarts it from the top, the old play fading out as for a cut pulse (below). It goes silent by itself at the sound's end. It ramps in and out like every voice. The speech voices' once mode stays as it is: it ignores pitch and the cue levels on purpose.
- **A pool for bursts.** A way for a cue to own several one-shot voices and trigger the next free one, taking the oldest playing when none is free, so several items appearing on one tick each sound. The pool's size is a constant.
- **Pulses cut with a fade.** A pulsed voice whose sound is longer than its fastest interval at its lowest pitch is no longer refused. When the next pulse is due while the last still plays, the old one fades out over a few milliseconds as the new one starts from the top. The slow pace plays a long sound whole; a fast one hears its start. Sounds that already fit behave exactly as before, so the existing cues are unchanged.
- **More room.** Raise `kMaxVoices` and `kMaxCues` to hold the two item cues, the appearance cue's pool, their preview twins, and a few tracked items later.

## Done when

- Engine tests (`test_cue_engine.cpp`), rendered into a buffer: a one-shot at a pan and a pitch, played once and silent after; its cue's switch and volume reaching it; a retrigger mid-play with no click; a pool of overlapping one-shots and the oldest taken when full; a long pulsed sound cut with a fade at a fast interval and played whole at a slow one; a sound that fits rendering as before.
- `docs/a11y/cues.md` describes the one-shot voice and the cut with a fade, replacing "A pulse must fit" with what now holds; ADR-0005 needs no change unless the decision itself moved.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`; the `verify` skill's quick level with `MELEE_A11Y_CUES=1`, the match cues starting as before.

## Comments
