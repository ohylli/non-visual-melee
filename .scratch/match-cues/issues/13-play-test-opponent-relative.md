# 13 Play test by ear: the opponent's position cue relative to you

Status: ready-for-human
Type: task
Blocked by: 12

The maintainer turns "Opponent's position cue relative to you" on and plays matches with NVDA against a CPU.

Listen for:

- Whether the opponent's side is always clear, and whether the pan floor is wide enough to hear as a side but narrow enough to hear the pan grow with distance.
- Whether the side distance suits: the pulse should reach full pan about where the opponent stops being an immediate threat.
- Crossing: cross-ups, rolls behind you and the opponent jumping over you should flip the pulse once, without flicker while you overlap. Too late a flip means a smaller crossing margin.
- Whether relative pitch says above or below you clearly, and whether the height distance suits platforms and jumps.
- Whether a relative opponent beside your absolute cue makes sense together, or confuses.

The tuning values live in `a11y.ini` under the opponent's heading. Findings become new issues here, and the defaults change in code.
