# 05 The opponent's position cue

Status: resolved (2026-10-14)
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

## Comments

### 2026-10-14, implementation (agent)

Implemented; the opponent's position cue pulses in every match beside yours. Three commits: two refactors the review of issues 01 to 03 recommended for this issue, then the feature.

- **Refactor 1, the value model's names.** The settings model's classes clashed with `CONTEXT.md`, which keeps "setting" for the port's options and calls cue settings a model of named values. Renamed: `Setting` to `NamedValue`, `Settings` to `ValueModel`, `SettingsGroup` to `ValueGroup`, `SettingsFile` to `ValueFile`, `SettingsRead` to `TextRead`, `SettingKind` to `ValueKind`, the handles' `setting()` to `named()`; files `value_model` and `value_file`, unit test suite `cue_settings`. The review suggested `Value` for one value; `CONTEXT.md` already has "Value" for what a row is set to, hence `NamedValue`. `CueSettings` keeps its name, the domain term. The file's text and the log lines are unchanged.
- **Refactor 2, `CueSystem`.** `cue_system.cpp` holds the engine and its output, the value model, the cue settings with their file and the match cues, opened together in `pc_a11y_init` and closed in `pc_a11y_shutdown`; `hooks.cpp` forwards the scene, master volume and match tick hooks to it. Behaviour is unchanged but for one order: the cues now stop just after a scene's announcement rather than just before.
- **The driver.** `opponent_position` in `position_cue.cpp`, a sibling of `your_position`. The stage mapping is shared code: `StageMapping` (edge pan, up and down range) and `stage_targets`, which both drivers call; each cue has its own copy of the three values. Silent outside a match, while paused, before GO (your input off, as for your cue), with no opponent and while the opponent is out; the snapshot already leaves out an opponent who is out, and the driver checks too. It plays on while you are out.
- **The voice** in `MatchCues` (now registering both cues in the file's order through member initialisers, since two `add_cue` calls as arguments would run in an unspecified order): "opponent position", a 400 Hz sine with a quarter of its octave, 5 ms raised-cosine rise (`fade_in`, new in `cue_sounds`), decaying at 30 per second over 70 ms, 10 ms faded tail, after the tick's 12 ms lead-in: 82 ms, 164 ms an octave down, so the interval's floor is 0.2 s (`kOpponentFastestInterval`). Boost 1.5.
- **Values** under "Opponent's position cue": `opponent_position.on`, `volume`, `boost` (1.5), `edge_pan` (0.6), `up_range` (1), `down_range` (1, at most 1, as yours), `interval` (0.4 s, 0.2 to 3 s). Descriptions read "Turns the opponent's position cue on or off." A file written before this issue gains the group at its end, as issue 02 built.
- **Tests.** `test_position_cue.cpp` gains the opponent's driver: the mapping on the opponent's position and its own values, the fixed interval at every distance, no opponent, knocked out, paused, before GO, you out, slow motion, and a change of opponent. A new suite, `match_cues` (`test_match_cues.cpp`), renders the voices: both cues register in order with your cue's mapping defaults, both voices fit, a change of opponent mid-pulse keeps one start line and one onset an interval on with the balance moved across, and the cue stops for a knockout and starts on the respawn platform while yours plays on.
- **The drive script.** `tools/a11y/match_opponent.drive` uses the debug VS match on Final Destination (`MELEE_DEBUG_VS=1 MELEE_DEBUG_VS_STOCKS=2`; `run_level.py` sets both): Link against a Mario on a human slot with no input, who stands still and never recovers. A deviation from "the CPU's stock loss": a real CPU fights back and recovers, and a run cannot make it lose a stock reliably, while the idle Mario falls the same way every run (seven presses of a dash and forward smash, in every run so far). The log shows the opponent's cue start at GO (pan 0.42), stop as Mario passes the bottom blast zone (pan 0.65, pitch 0.50) and start on the respawn platform a second later (pan -0.35, pitch 1.99).
- **Two pulse trains in the dump.** The analysis script could not see them: its onset detector missed every soft-attack pulse (a block between its two thresholds reset its quiet count), and a pulse starting inside another cue's sound is no onset. Fixed the first, and added `--band LOW HIGH` (eight-pole band-pass both sides) and `--threshold` (the onset level as a fraction of the peak, for the tick's broadband click that leaks into the low band). On the run's dump `--band 150 1000 --threshold 0.3` shows the opponent's train at 0.400 every interval, pan +0.42 to +0.65, then -0.34 after the respawn, at 400 Hz; `--band 1100 12000 --threshold 0.3` your tick at 1500 Hz, from 0.800 down to about 0.10 and back. `test_cue_dump.py` checks both bands on two overlapping synthetic trains.
- **Verification**, online level with `MELEE_A11Y_CUES=1`: build with the "Rollback sections" line; style; unit tests (`launcher_data` fails as known); a bounded `title` run with cues on (the scratch file gains the opponent's group); the sixteen menu scripts, `match_position.drive`, `match_opponent.drive`, the three `css_net.drive` runs and `lan_lobby.drive`, all 22 passing in one `run_level.py` call with no desync. Every script reaching GO logs both cues' starts; each `css_net.drive` side hears its own player at its own side and the other player opposite (a: yours -0.72, opponent +0.72; b the other way round). The maintainer's `a11y-cues.ini` was not touched.
- **For issue 06**, notes for the play test are in its file.
- **Left open.** Whether 0.4 s against your 0.8 s far pulse reads as one rhythm: they start together at GO and after every unpause, so every other opponent pulse lands on your tick until you come nearer. The spec leaves it to the play test; an interval such as 0.45 s, or starting the opponent's voice half a pulse late, would make them drift.
