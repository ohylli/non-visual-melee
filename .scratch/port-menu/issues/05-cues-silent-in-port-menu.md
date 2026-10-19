# 05 Cues are silent while the port menu is open

Status: ready-for-agent
Type: task
Blocked by: 02

`.scratch/port-menu/spec.md`, "Cues". While the port menu is open the game stands still and the tick hook stops, so the cue engine's voices go on as the last tick left them: a gap warning hums under the menu's speech, and a position cue keeps pulsing.

## Machinery

- **Stop on open.** When the port menu opens, every cue on the cue engine stops with its usual stop ramp. How the cue system learns of it while no tick runs is the spec's open question 7. Prefer a place the fork already runs each frame over a new hook.
- **Start on the next tick.** The first match tick after the menu closes starts the cues again from the match's state, as after any silence. Nothing is remembered across the menu.
- **No change to game state.** The cue system only reads, as before.

## Done when

- A match drive run with cues on (`MELEE_A11Y_CUES=1`, the cue trace or dump as the `verify` skill describes) opens the port menu mid-match with a pushed F1 (issue 02). The cue stop shows at the opening, nothing plays while the menu is open, and the cues start again after it closes.
- The `verify` skill's match scripts pass unchanged.
- A unit test of the cue system covers the stop and the restart, if the cue system's tests can reach it.
- `python tools/check_style.py` passes.
- `docs/a11y/cues.md` says the cues stand silent while the port menu is open, and why.
