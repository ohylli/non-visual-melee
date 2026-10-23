# 11 Play test online by ear: the four cues

Status: resolved (2026-10-23)
Type: task
Blocked by: 10

Split out of the play tests on 2026-10-14, after issue 06: those up to issue 10 are offline, and this one covers online for every cue. It needs a second device or a friend, so it waits until one is available.

The maintainer plays online matches with NVDA against a friend, or against a second instance on another device, and listens for:

- Your position cue following your own player on each side, and the opponent's position cue following the other player (carried from issues 04 and 06; the agent runs showed both on each side).
- A stalled session: whether the cues keep pulsing at their last place while the game waits for a stalled peer, until the session times out (issue 04's review). If it bothers, the engine could fade a voice whose targets have not changed for about a quarter second.
- Rollback: whether a misprediction corrected a tick or two later is heard in any cue as a jump or a flicker.
- The gap and ledge warnings starting and stopping as offline.
- Against a base port build too, if the friend plays one: the cues are the fork's own, so the base port side hears nothing and the session stays in sync.

Findings become new issues here. When it passes, CLAUDE.md's status line for match cues says the cues were play-tested online.

## Comments

### 2026-10-23: play-tested on LAN with a friend

The maintainer played LAN matches with NVDA against a sighted friend on a base port build and noticed no issues. The connection was good, so a stalled session and rollback corrections had little chance to show; if lag brings either up later, it becomes a new issue. No new issues.
