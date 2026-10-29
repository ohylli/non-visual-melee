# 06 Play test by ear

Status: resolved (2026-10-29)
Type: task
Blocked by: 05

The maintainer plays matches with NVDA and judges `.scratch/match-announcements/spec.md`.

## What to try

- **Damage, Every change.** A 1v1 against a CPU. Is the settle time right: too eager mid-combo, or too late after it? Is "45" alone clear in each voice?
- **Thresholds.** Switch each side to Thresholds and try intervals. Is the settled figure past the threshold better than the threshold itself? Is a heal's silence right?
- **Several opponents.** A free-for-all of four, then a 2v2. Are the labels quick enough by ear, "CPU 3" and "teammate P3"? With a name tag, does the tag read well?
- **Stocks.** "2 stocks" and "last stock", and the game's announcer at an elimination. Do they collide?
- **The time left.** A timed match: the minutes and 30 seconds, over the cues and the voices.
- **The voices together.** Do your voice, the opponents' voice, NVDA and the cues stay clear of each other in a busy moment? Is centring both voices enough, or is pan wanted?
- **Online.** A match against a friend or a second copy, if convenient.

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue, and CLAUDE.md's status line says match announcements were play-tested.

## Comments

### 2026-10-28, review (agent)

The review of issues 01 to 05 fixed what was plainly the code's own (its fix commit lists them) and leaves these for the maintainer, most severe first. Already left from the issues' own comments: the Multi-Man wireframes and their "0", single-player lives, 3-Minute Melee's countdown, Training's second figure after its menu, and a damage line taking a waiting stock line's place.

- **A slow, steady climb is said at every point.** Damage that rises one point at a time, further apart than the settle time, is said each time under Every change, the default for both sides: on Poké Floats a fighter off screen gave "CPU 1 78", "79", "80" (issue 01). Options: a hold-off after each line about a player (a tuning value), skipping a change of 1, or leaving it to Thresholds. Recommended: hear it in the play test first; if it nags, the hold-off.
- **Lines already waiting in a voice play on after GAME!, TIME! or a quit.** The end drops only figures still settling; with three or more players, lines queued behind the one playing go on over the game's announcer naming the winner and the results reader's start, or after a quit over the scene's announcement. The spec lets only a line already playing finish. Options: drop the voices' waiting lines at the end (`Voices::drop_waiting`, as Speech off does, reached through `Speech`), or leave it. Recommended: drop them.
- **Online, a KO that a rollback takes back leaves lines behind.** The stock line is said on its predicted tick (known in `docs/a11y/gameplay-state.md`, "Open questions"); after the rollback the count's return is said too ("3 stocks"), and the damage, zeroed by the KO, is said again once it settles. Options: take a rise outside a team match as said, silently, and zero the damage's last figure only once a stock is really gone; or hold stock lines a few ticks. Recommended: the silent rise, if it is ever heard.
- **"Announcer" in the names.** `MatchAnnouncer` and `match_announcer.*` use the word `CONTEXT.md` avoids, since the game has its own announcer, and the same files speak of "the game's announcer". Options: rename to `MatchAnnouncements` (files, tests, the `verify` skill's test name, CLAUDE.md), or keep the spec's name. Recommended: rename when the feature is next touched.
- **The player label's name tag duplicates results'.** `MatchAnnouncer::label` and `ResultsSpeech::opponent_name` both decode a tag and fall back to "P" and the port, and have drifted: results logs a tag with unknown characters or a malformed one, the match label drops them silently. Options: one shared helper from tag and port to a name, with the log; or leave it. Recommended: the helper, a small refactor across the two readers.

### 2026-10-29, play test by ear (maintainer)

Played with NVDA: matches with the announcements in both voices, with several opponents too, the settings adjusted along the way. No issues were noticed. The review's items above need no immediate action and stay as they stand, open for a later change if one is ever heard.
