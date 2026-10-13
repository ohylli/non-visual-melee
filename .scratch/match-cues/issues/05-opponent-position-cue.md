# 05 The opponent's position cue

Status: ready-for-agent
Type: task
Blocked by: 04

The opponent's position with the same mapping as yours, a different sound and a steady pulse. Read the spec's "The opponent's position cue" first, and the findings of issue 04's play test, which may have changed the shared mapping.

## Machinery

- **The driver**: your position cue's pan and pitch mapping applied to the opponent's position, with its own copy of the mapping values; a fixed interval. Silent with no opponent and while the opponent is knocked out or between death and respawn. Share the mapping code with your cue rather than copy it.
- **The sound**: a generated pulse in a different timbre and lower register than your tick, for example a short, rounder tone, so the two never blend.
- **Settings** registered: on or off, volume, boost, interval 0.4 s, and the mapping values with your cue's defaults (as tuned in issue 04).
- **Choosing the opponent** each tick is already in the snapshot (issue 03); a change of opponent moves the voice without restarting it.

## Done when

- Unit tests for the driver: the mapping on the opponent's position; the fixed interval; no opponent; the opponent knocked out; a change of opponent.
- The issue 03 drive script, or a new one, shows the opponent's cue starting and stopping with the CPU's stock loss and respawn, and the dump analysis shows two pulse trains at their intervals.
- `python tools/check_style.py` passes; the `verify` skill and `docs/a11y/cues.md` are updated.
