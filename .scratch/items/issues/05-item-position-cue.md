# 05 The item's position cue and within reach

Status: ready-for-agent
Type: task
Blocked by: 04

Spec: `.scratch/items/spec.md` ("The item's position cue", "The item cues' frame", "Ready for several items", "The previews"). Read it first, then `docs/a11y/cues.md` ("Drivers", "The opponent's position cue", "Preview") and `position_cue.hpp`.

## What to build

- **The driver.** A pure function from the snapshot's items and the cue's values to a pulsed voice's targets, or none, keyed by the tracked item's serial: the nearest loose item, chosen afresh each tick; a change of item moves the voice without restarting it. Its state (the side kept relative to you) is per serial, so tracking the nearest N items later is N voices and a setting; N is a constant 1 now, with no row.
- **Pace.** A steady interval, and a faster one while the tracked item is within reach. Both tuning values. With the cut with a fade (issue 01), a sound longer than the fast interval is cut there and plays whole at the steady pace.
- **Place.** The frame switch issue 04 added: relative to you with the crossing margin, so an item you walk over does not flicker between the ears, or on the stage with the item's position cue's own pitch range.
- **Silent** outside a match, while paused, before GO, with no loose item, and relative to you while you are out. Holding an item does not silence it.
- **The cue.** Registered with `add_cue` ("Item's position cue": switch, volume, Preview, boost a tuning value) with `item_position.wav`.
- **The preview.** A few pulses at the steady pace a little to one side, then a few at the within-reach pace, following the frame switch.

## Done when

- Driver tests with hand-written snapshots: the nearest loose item tracked and a held or thrown one skipped; a change of nearest item keeping the voice; the two paces at within reach and not; both frames, the crossing margin held and flipped; silence while paused, before GO, with none loose, while out relative to you; tracking on while you hold an item; the preview's moments.
- A drive run in Training: an item spawned from the menu, the player walking to it and turning to face it, with the trace showing the pace change at within reach and the pan following the walk; then a pickup silencing the cue.
- The menu scripts read the new rows, their step counts updated.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`; the `verify` skill's quick and menus levels with `MELEE_A11Y_CUES=1`.

## Comments
