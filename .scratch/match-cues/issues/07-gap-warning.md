# 07 The gap warning

Status: ready-for-agent
Type: task
Blocked by: 06

A held tone while you are in the air with nothing below you. Read the spec's "The gap warning" first, and `docs/a11y/gameplay-state.md` ("Is there ground below?", "Reading safely").

## Machinery

- **The snapshot** gains, for you in the air, the result of `mpCheckFloor` cast from just above your feet down to the bottom blast zone: a floor found or not, and its line. The cast runs in the tick hook, between ticks, where it leaves no trace; guard every line id against -1 before passing it on.
- **The driver**: on while you are in the air, in play, and the cast found nothing; off otherwise, after a hold of a few ticks so it does not flicker at a platform's edge. Silent while you are knocked out or between death and respawn.
- **The sound**: a generated held, centred tone, low and distinct from both position cues, at constant pitch and loudness, looped.
- **Settings** registered: on or off, volume, boost, and the hold in ticks.
- The game's own warning, sound 96, stays as it is.

## Done when

- Unit tests for the driver: ground, air with floor below, air with a platform below, air over a gap, the hold on leaving a gap, knocked out.
- A drive script on Final Destination and Battlefield walks off an edge and recovers, and jumps between Battlefield's platforms: the log shows the gap warning starting past the main stage's edge and stopping on return, and never over the platforms or the main floor.
- `docs/a11y/gameplay-state.md` closes its open question on raycasts for this use: the game's own cast, called in the tick hook.
- `python tools/check_style.py` passes; the `verify` skill and `docs/a11y/cues.md` are updated; `CONTEXT.md` gains "Gap warning".
