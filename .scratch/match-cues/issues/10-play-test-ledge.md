# 10 Play test by ear: the ledge warning and the four cues together

Status: ready-for-human
Type: task
Blocked by: 09

The maintainer plays matches with NVDA on several stages, offline. Online is issue 11.

Listen for:

- Whether the ledge warning starts at the right distance, and whether its side and closeness are clear on the ground, when jumping and when knocked across the stage.
- The handover to the gap warning past the edge.
- Silence at a platform's end over the main stage.
- All four cues at once: whether they stay apart, whether the mix gets tiring, and which cues the player wants on by default.
- Each cue's switch and volume in the tuning file.

Findings become new issues here. When it passes, CLAUDE.md's status line for match cues says "Play-tested by ear with NVDA", and the recovery cue (out of scope in the spec) can get its own spec.

## Comments

### 2026-10-14, notes for the play test (agent, from issue 09)

- **The file.** `%APPDATA%\melee-pc\a11y-cues.ini` gains a new heading at its end, "Ledge warning", the first time the new build starts; nothing already in the file changes.
- **The keys to tune**, under that heading:
  - `ledge_warning.on`, `ledge_warning.volume` and `ledge_warning.boost` (1 to 8 times, default 3): its level. In the agent's recording it is about as loud as the opponent's pulse at the edge, and about as loud as your tick where it starts.
  - `ledge_warning.distance` (5 to 100 units, default 30): where it starts. A dash covers 30 units in under half a second; Final Destination's floor is about 171 units wide, Battlefield's about 137.
  - `ledge_warning.pan_floor` (0 to 1, default 0.3): how far to the side it stays at the edge itself. At the warning distance it is fully to the side.
  - `ledge_warning.level_floor` (0 to 100%, default 30%): how loud it starts at the warning distance; it is full at the edge.
  - `ledge_warning.edge_offset` (2 to 10 units, default 5): how far past an end the test for a floor further down looks. Leave it unless an end is wrongly called deadly or safe.
- **The sound** is a two-note chord (660 and 880 Hz), pulsed five times a second at a steady pitch: the only chord among the cues, between the opponent's pulse and your tick in pitch.
- **When it sounds.** Within 30 units of an end of the floor below you past which nothing lies, on the ground and in the air, on that end's side, both sides at once on a small floor. It moves from fully to the side and quiet towards the centre and full as you near the edge. Silent at a platform's end over the main floor, at a floor's end against a rising wall, while you are knocked out, paused, before GO and on GAME!. Walking off, it stops on the tick the gap warning starts; coming back, it starts again near the edge.
- **Listen for, beyond the list above:**
  - **High jumps near an edge**: it follows the floor below you however high you are, so a jump far above the main floor's end still sounds it.
  - **Platforms reaching past the main floor** (Yoshi's Story's and Brinstar's side platforms, Kongo Jungle's left one): their outer ends count as deadly and sound; their inner ends do not.
  - **Your position cue near an edge**: its pan is ±0.6 at the main floor's ends, the ledge warning's 0.3 on the same side. Say whether the two together read clearly.
  - **The start at the warning distance**: the voice starts at 30% loudness, not from silence; say whether that is abrupt, or too quiet to notice.
- **What the drive runs could not reach:** the chord's pleasantness and level against the game's sounds and NVDA; knockback across the stage; stages other than Final Destination and Battlefield by ear (the deadly test was only logged on eleven stages); online.
