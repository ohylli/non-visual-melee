# 04 Item appearances: the announcement, the appearance cue, the frame switch

Status: ready-for-agent
Type: task
Blocked by: 01, 02, 03

Spec: `.scratch/items/spec.md` ("The appearance announcement", "The appearance cue", "The item cues' frame", "The settings rows", "The previews"). Read it first, then `docs/a11y/cues.md` ("The match cues", "The opponent's position cue", "Preview") and `match_announcer.cpp` for how a match reader starts and ends.

## What to build

- **New items.** A reader fed from the tick hook notices each item whose serial it has not seen this match, from GO to GAME! or TIME!, with the seen serials forgotten at each scene entry. Items present at GO are taken as seen, unsaid.
- **The announcement.** On the screen reader, queued: the item's name, or for several in one tick one announcement listing them in the snapshot's order, a repeated name said as often as it appears, joined by the helpers in `sentences.hpp`. Behind a new switch, **Item appearances**, in Match announcement settings after Time left, on by default, under the Match announcements switch.
- **The appearance cue.** A cue registered with `add_cue` ("Appearance cue": switch, volume, Preview, boost a tuning value), on the one-shot pool (issue 01) with `item_appearance.wav` (issue 02). Each new item triggers one one-shot at the place it appeared. Silent while paused (a pause stops the match's voices) and, relative to you, while you are out.
- **The frame.** A new switch, **Items relative to you**, on by default, in Cue settings after the opponent's relative switch, for both item cues. Relative to you: pan and pitch from the distances to you through the opponent cue's free functions (`relative_pan`, `relative_pitch` in `position_cue.cpp`), with the item cues' own copies of its distances as tuning values starting at its defaults. On the stage: your position cue's stage mapping and pan frame, under the shared Position cues follow the camera switch, with a pitch range of the item cues' own. A one-shot has no side to keep, so the crossing margin is issue 05's alone.
- **The preview.** Three one-shots a short pause apart: to your left, to your right, then above you, following the frame switch, with the preview twins every cue has.

## Done when

- Unit tests with hand-written snapshots: one new item said and played; a burst of three said once with three one-shots; an item seen at GO unsaid; nothing said twice for one serial; the switch and the cue's switch each off; silence while paused and, relative to you, while out; both frames' pan and pitch; the preview's moments.
- A drive run in Training spawns an item from the menu with `MELEE_A11Y_CUE_DUMP` and the trace: the log has the announcement and the appearance cue's play with its pan and pitch, and `tools/a11y/cue_dump.py` shows one onset on the expected side.
- The scripts that walk the Accessibility category (`launcher_a11y`, `port_menu_a11y`, `port_menu_voices` and any newer) read the new rows, their step counts past them updated.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`; the `verify` skill's quick and menus levels with `MELEE_A11Y_CUES=1`.

## Comments
