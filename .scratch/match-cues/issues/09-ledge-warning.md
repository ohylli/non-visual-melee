# 09 The ledge warning

Status: resolved (2026-10-14)
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

## Comments

### 2026-10-14, carried from the review of issue 07 (agent)

The maintainer passed issue 08 and asked that the review's refactoring notes reach this issue's implementer. Weigh them when you start; neither is required.

- **One "you are in play" test.** `GapWarning::tick` (`gap_warning.cpp`) repeats your position cue's silence guard (`!running || paused || !can_move || out`, `position_cue.cpp`), and `over_gap` then checks `running` and `out` again. The ledge warning would be a third copy. Suggestion: a shared `you_in_play(const A11yMatchState&)` beside the drivers, with `over_gap` reduced to `airborne && !below.found`.
- **Adding a cue to `MatchCues` takes about seven edits** (`match_cues.hpp`, `match_cues.cpp`): the `CueGroup`, a values struct, `add_*_values`, `mapping()`, a voice id, a getter and a `play` line, plus the group-count assert in `test_match_cues.cpp`. This issue adds two voices. Suggestion: a small per-cue bundle if the ledge warning makes the repetition painful; low priority.

Facts from the same review that bind this issue:

- **Casts only from the tick hook** (ADR-0006). The outward casts past each floor end are the same call as the gap warning's; a helper beside `match_floor_below` in `game_access.c` is the natural home, and every line id it is given or returns needs the -1 guard.
- **`below.y` can sit a few units above your feet in the air** (the cast starts at the collision box's bottom), so a drop computed from it can be negative.
- **"Nothing found" means a gap only in the air.** On the ground with an invalid floor index, `below.found` is false but there is no gap: check `airborne` too, or use `over_gap`.
- **The gap warning keeps sounding through a ledge hang**, by the maintainer's choice in issue 08: you are still in danger there. The handover from the ledge warning to the gap warning past a deadly edge should keep that.

### 2026-10-14, implementation (agent)

Implemented; a pulsed chord sounds on the left or right in every match while you are near a deadly end of the floor below you.

- **Where things live.** `game_access.c`: `match_cast_floor`, the one helper both casts use (the game's `mpCheckFloor` from a point down to the bottom blast zone, its line checked against the line count), `match_floor_end`, and the ends in `A11yFloorBelow` (`A11yFloorEnd`: x, y, deadly). `ledge_warning.hpp/.cpp`: the driver, a pure function returning the two voices' targets. `match_cues.cpp`: the "Ledge warning" group, the pulsed voices "ledge left" and "ledge right" and their sound. No new hook.
- **The deadly test.** An end is deadly when no wall rises from it and the cast from 1 unit above the end's height, the edge offset outwards, meets no floor. The wall test reads the line joined at the end past the floor lines (`mpLineGetNext` or `mpLineGetPrev`, the walk `mpFloorGetRight` and `mpFloorGetLeft` make, bounded): if its far end is higher, the floor ends at a step, which no walk leaves by, and a cast past it would start inside the stage. Not in the spec; added after a probe found such ends against Princess Peach's Castle's tower and on Hyrule Temple. Every line id is guarded, and `mpLib_80054ED8` is only called on a valid one, since it spins forever on a bad one.
- **The edge offset is a setting, as the issue asked**: `ledge_warning.edge_offset`, default 5 units, 2 to 10. It must clear the unit a floor reaches past its end where another line joins it (where the gap warning starts), and stay under the 10.8 units from a Battlefield side platform's end to the main floor's, or the platforms' ends turn deadly. The snapshot needs it, so `CueSystem::tick` now fills the snapshot itself with `MatchCues::edge_offset()`, and `pc_a11y_match_tick` hands it the port, the online flag and the held mask.
- **Measured** with a temporary probe that logged every floor surface's ends on eleven stages, recorded in `gameplay-state.md`, "How far to the edge?": the main floors' ends are deadly on all of them and the platforms over the main floor safe; the outer ends of Yoshi's Story's and Brinstar's side platforms and Kongo Jungle's left one reach past the main floor and are deadly, which is right; Mushroom Kingdom's pits make deadly ends mid-stage.
- **The mapping** is the spec's: pan from fully to the side at the warning distance (30 units) to the pan floor (0.3) at the edge, gain from the level floor (30%) to full, linear in the horizontal distance, held at the edge past it. No hold: the distance moves smoothly and the voice only starts and stops at the warning distance, at the handover and when you leave play.
- **The handover** came out as the spec hoped with no extra code: walking off, the floor below vanishes a unit past the end, and the ledge warning stops at pan 0.30, gain 1.00 on the same tick the gap warning starts; coming back, it starts again near the edge while the gap warning holds its sixth of a second. Through a ledge hang only the gap warning sounds, as issue 08 kept it.
- **The sound**: 660 and 880 Hz together (a just perfect fourth, as the Star Fox 64 fork's beside chord, higher), 90 ms after 12 ms of lead-in, pulsed every 0.2 s at a fixed pitch. Its register lies between the opponent's 400 Hz pulse and your 1500 Hz tick on the floor, and it is the only chord.
- **Level**: boost 3. In a dump of `match_ledge.drive` with the position cues switched off, 0.2 s windows: about -21 dB at the edge (level with the opponent's pulse, 2 dB over the hum's -23) and -31 dB at the warning distance (near your tick). Boost 2 measured 4 dB less.
- **The review's suggestions.** One "you are in play" test: done, `you_in_play` in `match_state.hpp`, used by your position cue, the gap warning and the ledge warning; `over_gap` is now `airborne && !below.found`, and its test no longer asks it about a knocked-out fighter. The per-cue bundle in `MatchCues`: not done. The ledge warning took the same seven edits plus a second voice; the repetition is plain and each cue's values differ, so a bundle would save little until a cue of another shape arrives.
- **Tests.** `ledge_warning` (new suite): each side, the pan and gain at the warning distance, halfway and at the edge and steadily between, the mapping's values, air as ground, a unit past the end, both ends of a small floor, a platform's safe ends and one deadly end, no floor below in the air and on the ground, out, paused, before GO, outside a match, slow motion and the respawn platform. `match_cues` gains the group and its seven values in order, the edge offset following its value, the start and stop lines on each side and the handover to the gap warning on one tick, both voices at once, and the pulse rendered: five onsets 0.2 s apart in a second, on its side.
- **Drive scripts.** `tools/a11y/match_ledge.drive` (Final Destination) and `tools/a11y/match_ledge_battlefield.drive` (Battlefield), both in the debug VS match, with exact line counts in `run_level.py`. On Battlefield no press stops Link exactly at a platform's end: a 150 ms dash stops him about two units short (-55.9 against -57.6), and longer ones or a tap after slide him off. Two units short is well inside the main floor's warning distance (12.5 units from its end), so the contrast the issue asks for holds: silent on the platform, the warning at once on dropping through.
- **Verification**, online level with `MELEE_A11Y_CUES=1`: build with the "Rollback sections" line; style; unit tests (`launcher_data` failing as known); a bounded `title` run with cues on, its scratch settings file gaining the "Ledge warning" group with its seven values; the six match scripts through `run_level.py`, the four gap and ledge ones twice; the sixteen menu scripts and the four online runs, all 20 passing in one call, no desync. The maintainer's `a11y-cues.ini` was not touched.
- **Left open.** The play test (issue 10). Odd stages stay outside the spec: a moving floor's ends are read fresh each tick, but nothing was tried on one. The warning follows the floor below however high you are, so a jump far above the main floor near its end sounds it; the spec asks for that, and the play test can say whether it helps.
