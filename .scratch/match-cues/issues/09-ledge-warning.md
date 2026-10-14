# 09 The ledge warning

Status: ready-for-agent
Type: task
Blocked by: 08

A pulsed tone on the left or right while you are near a deadly end of the floor below you, on the ground and in the air. Read the spec's "The ledge warning" first, and `docs/a11y/gameplay-state.md` ("How far to the edge?").

## Machinery

- **The snapshot** already holds the floor below you, `A11yMatchState.below`, filled by issue 07 (see its "For issue 09" comment): on the ground `coll_data.floor.index`, in the air the line the gap warning's cast found. From it, the floor's left and right ends (`mpFloorGetLeft`, `mpFloorGetRight`), and for each end whether a downward cast a few units outward finds a floor. Casts in the tick hook only; line ids guarded.
- **The driver**: for each deadly end within the warning distance, a voice on that side: pan fully to that side at the warning distance moving toward the centre as you close in, stopping at the pan floor at the edge; loudness from the level floor at the warning distance to full at the edge. Two voices when both ends of a small floor are close. Silent with no floor below, which hands over to the gap warning, and while knocked out or between death and respawn.
- **The sound**: a generated tone pulsed at a fixed rate of about 5 Hz, distinct from the other cues.
- **Settings** registered: on or off, volume, boost, warning distance 30 units, pan floor 0.3, level floor 30%, the outward offset of the deadly test.

## Done when

- Unit tests for the driver: a deadly end on each side, both ends, a safe end over the main stage, ground and air, the pan and loudness at the warning distance and at the edge, no floor below, knocked out.
- A drive script on Final Destination walks to each edge and jumps along the stage; on Battlefield it stands at a side platform's end: the log shows the ledge warning on each edge's own side as the fighter nears it, rising to the edge, fading as the gap warning starts past it, and silent at the platform's end.
- `python tools/check_style.py` passes; the `verify` skill and `docs/a11y/cues.md` are updated; `CONTEXT.md` gains "Ledge warning".
