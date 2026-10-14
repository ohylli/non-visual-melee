# 08 Play test by ear: the gap warning

Status: ready-for-human
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
