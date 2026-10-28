# 04 The time left

Status: ready-for-agent
Type: task
Blocked by: 01

Spec: `.scratch/match-announcements/spec.md`, "The time left".

## What to build

- **The snapshot** gains the timer: `timer_seconds` and `unk_2C` from `VsSceneState`, and from `StartMeleeRules` whether the timer is on and whether it counts up.
- **On the screen reader, queued**: "2 minutes left", "1 minute left" at each whole minute of a countdown, and "30 seconds left", on the tick the HUD shows that time (open question 3). The match's starting minute is not said. A timer counting up says nothing; the game's five-to-one countdown is left alone.
- **The row**, last of the match announcements: "Time left", a switch, on by default.

## Done when

- The announcer's unit test covers each minute, 30 seconds, the starting minute, a limit that is not a whole minute, counting up, no timer, a pause (nothing said twice), and the switch.
- A drive run of a timed match shows the lines at the right times (the `verify` skill names how to reach one).
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.
- `docs/a11y/gameplay-state.md` has the timer as the spec found it, with the answer to open question 3.
