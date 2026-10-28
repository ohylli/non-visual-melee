# 01 The match snapshot and damage on every change

Status: ready-for-agent
Type: task
Blocked by: `.scratch/voices/issues/02-voice-engine.md`

Spec: `.scratch/match-announcements/spec.md`. Read it first, then `docs/a11y/gameplay-state.md`, `.scratch/voices/spec.md` ("Queueing") and `.scratch/training-menu/issues/01-reader.md`, an earlier reader polled from the tick hook.

## What to build

- **The snapshot.** `game_access.c` reads, per slot: the damage the HUD shows (`Player_GetDamage`), stocks, whether its HUD panel shows (open question 1), controller port, human or CPU, team, the name tag's text (as `results_opponents` reads it), and whether the fighter is KO'd (open question 2); and for the match: the phase (`VsSceneState.unk_0`), whether the HUD is enabled (GO), teams, and "you" (`match_your_slot`, as the cues use). Issue 04 adds the timer.
- **The announcer.** A new reader fed from `pc_a11y_match_tick` behind `game_hook_may_speak()`, as Training's menu and results are. No new hook.
- **When.** From GO to GAME! or TIME!. Each player's damage at GO is the baseline, unsaid. At GAME! or TIME! damage waiting to settle is dropped and logged as dropped.
- **Who.** You in your voice; everyone else with a HUD panel in the opponents' voice, teammates included (issue 03 adds their switch). No "you": everyone in the opponents' voice. A line's subject is the player's slot.
- **Labels.** Decided at GO and kept: with two or more players besides you, each line in the opponents' voice starts with the player label: the decoded name tag for a human with one, offline only; else "P2" by port, or "CPU 3" by slot; a teammate's starts with "teammate".
- **Damage, Every change.** Said once it has held still for the settle time (a tuning value), with no maximum wait, whenever it differs from the last figure said for that player: "45", "P2 45". A KO resets the last figure to 0 silently.
- **The Match announcements switch.** The first row of the match announcements, after the voices' rows, on by default, under Speech. For now damage runs in Every change for everyone; issue 02 adds its modes.

## Done when

- The announcer's unit test (fake clock, snapshots by hand) covers: settling through a combo, a rise, a fall, a heal's run of drops said once, a KO reset, the GO baseline at 300%, GAME! dropping a pending figure, labels (one opponent, two, a teammate, a name tag, a CPU, no "you", fixed at GO), a fighter without a panel, the switch.
- A drive run of the debug match (`MELEE_DEBUG_VS=cpu` with stocks) against a CPU that attacks shows your damage and the CPU's in the log, each in its voice, unlabelled.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`; the `verify` skill's quick level with `MELEE_A11Y_CUES=1`, as the tick hook changed.
- `docs/a11y/gameplay-state.md` has the HUD's damage, stocks and labels as the spec found them, with the answers to open questions 1, 2 and 6.
