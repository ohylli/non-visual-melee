# 08 Play test by ear: the gap warning

Status: resolved (2026-10-14)
Type: task
Blocked by: 07

The maintainer plays matches with NVDA on several stages.

Listen for:

- Whether the warning starts early enough to react to, and whether it is clear without being alarming.
- Whether it masks the position cues or the game's own sound 96 near the bottom blast zone, and whether the two warnings together make sense.
- Flicker at a platform's edge or when jumping past the stage's edge and back.
- Stages where it is wrong: stages with gaps in the middle, moving platforms, Brinstar's lava.

Findings become new issues here. When it passes, issue 09 starts.

## Comments

### 2026-10-14, notes for the play test (agent, from issue 07)

- **The file.** `%APPDATA%\melee-pc\a11y-cues.ini` gains a new heading at its end, "Gap warning", the first time the new build starts; nothing already in the file changes.
- **The keys to tune**, under that heading:
  - `gap_warning.on`, `gap_warning.volume` and `gap_warning.boost` (1 to 8 times, default 1): its level. In the agent's recording it sits a little under the opponent's pulse and above your tick; a held tone can tire the ear sooner than a pulse, so say if it is too loud or too quiet.
  - `gap_warning.hold` (0 to 60 ticks, default 10, a sixth of a second): how long it sounds on after a floor is below you again. Raise it if it stutters at an edge, lower it if it lingers after you are back.
- **The sound** is a low, hollow hum at 120 Hz (with overtones at 360 and 600 Hz), centred, at a steady pitch and level: lower than both position cues and the only one that is held rather than pulsed.
- **When it sounds.** In the air with no floor anywhere below you, platforms included: past the main floor's ends (about a unit past them), under the stage, and from a platform's end only if nothing lies beneath it. It stops a sixth of a second after a floor is below again, and at once when you are knocked out, the match is paused, or on GAME!. It stays off over Battlefield's platforms, anywhere above the main floor and on the respawn platform. The game's own warning, sound 96, still plays much later in the fall.
- **Listen for, beyond the list above:**
  - **Hanging on a ledge**: you are in the air with nothing below, so the hum plays through the whole hang. Say whether that helps (you are still off the stage) or gets in the way; silencing it while hanging is a small change.
  - **Jumping just past an edge and back**, the flicker the hold is for.
  - **The opponent below the floor**: the hum's overtones sit in the opponent's pulse's low range; only held against pulsed keeps them apart.
- **What the drive runs could not reach:** the hum's pleasantness and level against the game's sounds and NVDA; stages other than Final Destination and Battlefield (the spec names gaps in the middle, moving platforms and Brinstar's lava, none of them tried by the agent); online.

### 2026-10-14, review of issue 07: left for the maintainer (agent)

The review fixed what was plainly the agent's (the cast skips a platform being dropped through, as the game's own collision step does; comments on `A11yMatchState.below`; `run_level.py` robust to a missing transcript). These need your judgement, most important first:

- **An ADR for calling the game's own cast.** The fork now calls `mpCheckFloor`, which sets the joints' temporary marks and clears them before returning; the review traced every write and found the call leaves the collision data as it found it, but CLAUDE.md's "Online compatibility" still says nothing else the fork does may change game state, and the rejected alternative (a line search in fork code) is recorded only in `gameplay-state.md`. Issue 09 adds more casts. Recommendation: a short ADR-0006, "Floor tests call the game's own cast from the tick hook", and a one-line pointer to it in CLAUDE.md.
- **The hum through a ledge hang** (already under "Listen for"): hanging is in the air with nothing below, so recovering to the ledge is one unbroken hum, and only the game's grab sound marks the catch. Recommendation: decide by ear; silencing it while hanging is a small check on the cliff motion states in `over_gap`.
- **The hum's overtones and the opponent's pulse** (already under "Listen for"): the 360 and 600 Hz harmonics sit in the opponent's 200 to 800 Hz range. Recommendation: decide by ear; if they blur, drop the fifth harmonic or lower the fundamental to about 100 Hz.
- **One "you are in play" test, before issue 09.** `GapWarning::tick` repeats `your_position`'s silence guard (`!running || paused || !can_move || out`), and the ledge warning would be a third copy. Recommendation: a shared `you_in_play(match)` beside the drivers when issue 09 starts; it crosses the position cue's module, so it was left.
- **Adding a cue to `MatchCues` takes about seven edits** (group, values struct, `add_*_values`, `mapping()`, voice id, getter, `play` line). Issue 09 adds two voices. Recommendation: low priority; consider a per-cue bundle only if issue 09 makes it painful.
- **A hum through a scene load.** A match that ends with no tick reporting it over (an online disconnect while over a gap) keeps the hum until the next scene has loaded; GAME!, a pause quit and a knockout silence it in time, and the position cues already behave this way. Recommendation: leave it unless heard; the fix would stop the cues when a scene starts loading.

### 2026-10-14: play-tested by ear

The maintainer played matches with NVDA offline on Final Destination, Battlefield, Jungle Japes (its two small side platforms) and Brinstar, and the warning worked on all four. No new issues.

- **The sound is good and distinct** from both position cues.
- **It gives time to react**, though a recovery still has to be quick.
- **No flicker** was noticed at a platform's edge or jumping past an edge and back, so the hold stays at 10 ticks.
- **The hum through a ledge hang stays**: you are still in danger while hanging, so the tone is right there.
- **The overtones against the opponent's pulse** were not raised; the two stay apart by ear, so the sound stays as it is.
- **The ADR is accepted**: ADR-0006, "Floor tests call the game's own cast from the tick hook", with a pointer in CLAUDE.md's "Online compatibility".
- **The refactoring notes** (a shared "you are in play" test, a per-cue bundle in `MatchCues`) move to issue 09's Comments for its implementer to weigh.
- **Not tested:** online (issue 11), and the hum through a scene load after an online disconnect, left as low priority.
