# 06 Play test by ear: the opponent's position cue

Status: resolved (2026-10-14)
Type: task
Blocked by: 05

The maintainer plays matches with NVDA against a CPU, and online when a friend is available.

Listen for:

- Whether the opponent's sound is clearly different from yours when both play at once.
- Whether you can tell where the opponent is, and where they are compared with you, from two pans on the same stage mapping. If not, panning the opponent relative to you (out of scope in the spec) becomes a new issue.
- Whether the two pulse trains fall into step and sound like one rhythm (the spec's open question), and whether 0.4 s is the right steady pulse.
- The cue going quiet while the opponent is knocked out, and back at respawn.
- Online, carried over from issue 04: your own cue following your player on each side, and whether a stalled session keeps the ticks pulsing at their last place (issue 04's review).

Findings become new issues here. When it passes, issue 07 starts.

## Comments

### 2026-10-14, notes for the play test (agent, from issue 05)

- **The file.** `%APPDATA%\melee-pc\a11y-cues.ini` gains a new heading at its end, "Opponent's position cue", the first time the new build starts; nothing already in the file changes. Deleting the file writes every value afresh at its defaults.
- **The keys to tune**, under that heading:
  - `opponent_position.on`, `opponent_position.volume` and `opponent_position.boost` (1 to 8 times, default 1.5): its level beside your tick (boost 2). A guess: the pulse is longer and lower than the tick, so it was given less lift.
  - `opponent_position.interval` (0.2 to 3 s, default 0.4): its steady pace. At 0.4 it falls in step with your slow 0.8; try 0.45 or 0.35 to hear whether drifting apart helps.
  - `opponent_position.edge_pan`, `up_range` and `down_range` (defaults 0.6, 1 and 1, as yours): the same mapping, set separately, so the opponent's could for example sit wider than yours.
- **The sound** is a short, round 400 Hz tone; yours is a 1500 Hz click. Say if they are hard to tell apart, or if either masks the other when both sound at once.
- **When it sounds.** From GO, with yours; silent while paused, on GAME! and while the opponent is knocked out, back as they appear on the respawn platform (high up, so its pitch starts high and sinks as the platform comes down). It keeps playing while you are knocked out. It follows the nearest opponent; with more than one, it jumps to whoever is nearest without breaking its rhythm.
- **What the drive runs could not reach, so listen for it:**
  - Whether the two cues together are pleasant, clear and distinct at all, and against the game's sounds and NVDA: agent runs are silent, and the dump only shows their own levels (about -33 to -36 dB average for each in its own band at the defaults).
  - Whether you can tell where the opponent is relative to you from two pans on one mapping, the spec's question about panning the opponent relative to you.
  - A real CPU or player fighting back: the run's opponent stood still.
  - More than one opponent, and teams: the snapshot's choice of the nearest was tested with hand-written snapshots only.
  - Online with a friend, carried from issue 04: each side's cues followed the right players in the agent runs.

### 2026-10-14, left from the review of issue 05 (agent)

The review of 1940185..4e2c76e fixed what was plainly the agent's in its own commit. Two things need your ear first:

- **The two cues' registers overlap at the extremes.** The opponent's pulse is 400 Hz with a quarter of its octave: on the default mapping its note runs from 200 to 800 Hz and its overtone up to 1600 Hz, and with `opponent_position.up_range` at 2 the note itself reaches 1600 Hz. Your tick runs from 750 to 3000 Hz. On every respawn the opponent sits near 800 Hz (pitch 1.99 in the run's log), so against your tick on the floor at 1500 Hz only the timbre keeps them apart. Options: leave it, or lower the pulse to about 250 to 300 Hz so the ranges stay apart at the default mapping. Recommendation: listen for it on a respawn while you stand on the floor; lower the pulse only if the two blur there.
- **With more than one opponent, a knockout can go unheard, and the cue may wander while you are out.** The snapshot follows the nearest opponent in play, so knocking out the one the cue follows gives no silence: the pulse jumps in pan and pitch to the next nearest, and nothing says it is someone else. While you are knocked out the nearest is measured from where you were last, so in a match of three or four it can switch between opponents until you respawn. Options: leave it; keep following the same opponent while you are out; or mark a change of opponent (a short gap, or a different first pulse). Recommendation: listen in a three- or four-player match first; the spec's "list of one" design may want a decision before the gap and ledge warnings arrive.

### 2026-10-14: play-tested by ear

The maintainer played matches with NVDA against a CPU, offline, and found nothing major. No new issues.

- **New default from the tuning file:** `opponent_position.boost` 3 (was 1.5), too quiet before; now in the code. A tuning file already written keeps its own value; deleting it brings the new default.
- **The two cues are easy to tell apart** when both play at once.
- **The rhythm is not a problem:** the two pulse trains falling into step did not bother the ear, so the spec's open question is answered and the 0.4 s interval stays. The pitch overlap on a respawn, left by the review, was not raised either.
- **Online was not tested.** It moves to issue 11, one online play test for all four cues after issue 10, when a second device or a friend is available. It takes this issue's online item and issue 04's: each side's cues following the right players, and a stalled session.
- **Not raised:** more than one opponent, and panning the opponent relative to you. The review's recommendation for several opponents stands: listen in a three- or four-player match when it suits.
