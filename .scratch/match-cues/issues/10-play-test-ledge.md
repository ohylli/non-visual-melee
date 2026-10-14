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

### 2026-10-14, left by the review of issue 09 (agent)

The review fixed comments, one named constant and docs; nothing a player hears changed. These need the maintainer's judgement, most of them by ear:

- **Both sides at once may sound as one.** On a small floor with both ends deadly (Mushroom Kingdom between its pits, Poké Floats), both voices start on the same tick with the same chord at the same rate, so their pulses line up and can blend into one sound in the middle. Options: start the right voice half an interval (0.1 s) later so the sides alternate, or raise the right chord a step. Recommendation: listen on such a floor first; if the sides blend, alternate them.
- **No margin at the warning distance.** The voice starts and stops at the same distance, so a fighter shuffling right at 30 units starts it again each time, at 30% loudness. Option: stop it only a couple of units further out than it starts. Recommendation: change it only if the play test hears it flicker.
- **Switched-off floor lines.** The walk to a floor's end follows the game's own `mpFloorGetLeft` and `mpFloorGetRight`, which do not check whether the next floor line is switched on. Where a stage switches lines off (Pokémon Stadium's transformations), an end could be placed past the real floor, and the deadly test casts from there. Option: stop the walk at a line the game reports switched off. Recommendation: listen on Pokémon Stadium through a transformation first.
- **The handover is a cut, not a fade.** Walking off, the ledge warning stops at full loudness with the engine's short stop ramp on the tick the gap hum starts. A clean switch may mark leaving the floor better than a fade; say which you prefer.
- **The snapshot's inputs** (code only). `CueSystem::tick` now passes the local port, the online flag and the held mask through to the snapshot and asks `MatchCues` for the edge offset. One struct of snapshot inputs, with `MatchCues` filling its own part, would keep the next such input to one place. Recommendation: wait for a second input before adding it.
