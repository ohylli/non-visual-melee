# 05 Cues are silent while the port menu is open

Status: resolved (2026-10-19)
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

## Comments

### 2026-10-19, implementation (agent)

Implemented: the cues stand silent while the port menu is open and start again on the first tick after it closes.

- **Open question 7, how the cue system learns the menu is open: from the port menu's frame hook.** `pc_a11y_port_menu_frame(open)` (issue 04) runs at every frame boundary, open or not, also while the game stands still, so no new hook. In `hooks.cpp`, before the speech gate, each frame it is open calls `CueSystem::port_menu_open`, which calls `MatchCues::silence`: the engine's `stop_all`, each voice with its stop ramp, and the gap warning's hold forgotten (`GapWarning::forget`). It is called every open frame rather than on the transition, so nothing is tracked; a stop of a silent voice logs nothing. Before the gate because stopping cues is never wrong, and before the speech so the stops come ahead of the opening in the log.
- **Nothing remembered across the menu.** `MatchCues::tick` already starts any voice its driver gives targets, so the restart needed nothing; the one thing kept between ticks, the gap warning's hold, is forgotten at the silence, or a hum holding on as the menu opened would come back after it for the rest of its hold.
- **Unit test** in `match_cues` (`silence_stops_every_voice_and_the_next_tick_starts_afresh`): your, the opponent's and a holding gap voice stop with one log line each, the mix is silent past the ramp, a second silence logs nothing, and the next tick starts both position cues pulsing at once and leaves the gap warning off. `CueSystem` itself stays out of the tests' reach (it opens a device and reads the game), so the test covers `MatchCues::silence`, which is all `port_menu_open` does. Dropping the `forget` fails it.
- **Done when, the drive run.** A hand-written run (not kept; issue 06 writes the script): the debug VS match on Final Destination (`MELEE_DEBUG_VS=1 MELEE_DEBUG_VS_STOCKS=2 MELEE_A11Y_CUES=1`, dump and trace, `drive.py --muted`), Link standing at GO. `key F1`: `cue your position stop`, `cue opponent position stop`, `cue ledge left stop`, then the opening `"PC settings. Graphics, tab, ..."`, all on one frame; two seconds with no cue line; `key Escape`: `"Back to game"`, then the three starts with the same targets on the first tick. Then Link ran off the left edge (`cue gap start`, the ledge stop), `key F1` stopped your, the opponent's and the gap voice, and `gamepad b` brought all three back, the gap warning since Link still hung over the gap. `cue_dump.py --window 0.5` showed silent windows for both menu openings between pulsing ones.
- **What issue 06 can wait on.** The log, not the trace: each sounding voice's `[a11y] cue <name> stop ...` comes just before the opening's speech line, and its `start` just after `"Back to game"`. The trace is written only on ticks, so it has no rows while the menu is open and its tick numbers run on without a gap; the dump shows the silence (`cue_dump.py`, silent windows). A script cannot fail on a line too many, so a check that nothing starts while the menu is open belongs in `run_level.py`, as for the gap scripts (no `cue ... start` between the opening and `"Back to game"`).
- **Checked.** Build, `python tools/check_style.py`, unit tests (`launcher_data` failing as known), and with `MELEE_A11Y_CUES=1` the six match scripts, the menus level and the online level: 29 of 29 passed (`match_position` with its six `cue your position st` lines, `match_gap` with two `cue gap st`).
- **Docs.** `docs/a11y/cues.md` ("Lifecycle and switches": silent while the port menu is open, and why), a pointer from `docs/a11y/rmlui-screens.md`'s frozen game, the `verify` skill (what a run with the menu opened mid-match logs, dumps and traces; the unit test).
- **Left open.** Nothing for this issue. CLAUDE.md's match cues status line does not mention the port menu; issue 06 writes the port menu's status line and may name it there.
