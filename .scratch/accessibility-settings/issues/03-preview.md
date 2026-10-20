# 03 Preview: a short sample of each cue

Status: ready-for-agent
Type: task
Blocked by: 01

`.scratch/accessibility-settings/spec.md`, "Preview". A way for fork code to play about two seconds of a cue at a fixed, typical moment in a match, outside any match.

## Machinery

- **The call.** `CueSystem` gains a preview of a cue by its id (the id `add_cue` takes) and a stop. Each cue supplies its own fixed targets beside its driver: the position cues a few slow pulses from the centre at middle height, the gap warning its hum ramped in and out, the ledge warning a few pulses on the left then on the right. Exact targets are set by ear; the tunable values live in code only.
- **Levels.** A preview ignores the cue's switch, the Cues switch and the Accessibility switch, and honours the master volume and mute, Cue volume and the cue's own volume and boost.
- **Restart and stop.** A second preview restarts it; a stop ends it with the usual ramp.
- **The port menu.** Its per-frame silencing (`silence_for_port_menu`) and `scene_entered` stop only the match's voices, so a preview plays on in the port menu and in the launcher.
- **Online.** The preview plays on the cue engine only, never through the game's sounds.

## Done when

- Unit tests on the engine's mix: a preview sounds with every switch off, is silent at a volume of 0, restarts, stops with a ramp, and survives the port menu's silencing, while the match's voices are still silenced.
- A hand run through the cue dump (`MELEE_A11Y_CUE_DUMP`, `tools/a11y/cue_dump.py`) shows each preview's sound, triggered through whatever test seam the implementer adds, removed afterwards or kept as a debug switch named in the `verify` skill.
- The `verify` skill's quick level passes, and `python tools/check_style.py`.
- `docs/a11y/cues.md` describes Preview and why the port menu silences only the match.
