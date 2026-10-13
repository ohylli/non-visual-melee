# 03 The match tick hook and your position cue

Status: ready-for-agent
Type: task
Blocked by: 01, 02

The first cue in a match, with the hook and the snapshot every later cue reads. Read the spec's "When cues sound", "Who is you, and who is the opponent", "The stage frame", "Your position cue" and "How the fork attaches" first, and `docs/a11y/gameplay-state.md` ("Reading safely", "When to read: after the fighters move", "Position and motion", "Stage identity, blast zones, camera and points").

## Machinery

- **The tick hook**: `pc_a11y_match_tick(uint64_t held_mask)`, one line beside `pc_slp_tick_end` in `gm_RunSimTick` (`src/melee/gm/gmscene.c`), inside the base port's existing `TARGET_PC` block, declared in `a11y_hooks.h`. Silent while `pc_net_resim()` is true. It also calls the cue settings' `poll()`.
- **Match or not**: the hook runs in every scene that uses `gm_RunSimTick`, menus included, so the snapshot says whether a match with fighters is running; outside one, nothing sounds.
- **Pause and GAME!**: the fighters' bit in `held_mask` for more than a few ticks fades every voice out; the fighters moving again fades them back in. Look for a direct pause flag that tells pause from a slow-motion mode's skipped ticks (spec, open questions), and use it if one turns up; record the finding in the primer either way.
- **The match snapshot**, filled in C in `game_access.c` (compiled with the game's bitfield layout): you and the opponent, each with position, ground or air, knocked out or between death and respawn (`motion_id` 0 to 11), and team; the stage frame (the floors' extent, side and bottom blast zones, camera box top). You are port 1 offline and `pc_net_local_player()` online, the slot's main fighter; the opponent is the nearest other main fighter in play, not on your team, kept as a list of one.
- **The driver**, a pure C++ function from the snapshot and the cue's settings to your voice's targets:
  - Pan piecewise linear in x: main stage centre 0, its edges ±edge pan, side blast zones ±1.
  - Pitch piecewise linear in octaves of y: main floor 0, camera box top + up range, bottom blast zone − down range.
  - Interval on a log scale from slow at the far distance and beyond to fast at the near distance; slow with no opponent.
  - Silent while you are knocked out or between death and respawn.
- **The sound**: a generated tick, about 12 ms of lead-in silence then a damped sine of about 20 ms (the Star Fox 64 fork's aim click, `AimCue_GenerateClick`). It must fit the fast interval's floor.
- **Settings** registered: on or off, volume, boost (issue 02's helper), and edge pan 0.7, up range 1 octave, down range 1 octave, slow interval 0.8 s, fast interval 0.08 s, far distance 120 units, near distance 10 units, each with a sensible range.

## Done when

- Unit tests for the driver against snapshots: pan at the stage's centre, edges, past the edges and at the blast zones; pitch at the floor, the camera top and the bottom blast zone; the interval at, between and beyond the near and far distances; no opponent; knocked out.
- A drive script under `tools/a11y/` starts a Training match on Final Destination, walks to each edge, jumps off and comes back, then pauses and unpauses. Its log shows your cue starting at the match, stopping and starting with the pause, and stopping at the scene change; the trace's pan and pitch follow the walk; the analysis script on the dump shows the balance swinging and the pulse speeding up as the fighter nears the CPU.
- `python tools/check_style.py` passes; the footprint grep shows the tick hook.
- The `verify` skill lists the script and its expected lines.
- `docs/a11y/gameplay-state.md` records the tick hook as built and the pause finding; `docs/a11y/cues.md` describes the match snapshot and the drivers.
- `CONTEXT.md` gains "Position cue"; CLAUDE.md's accessibility status gains the line for match cues.
