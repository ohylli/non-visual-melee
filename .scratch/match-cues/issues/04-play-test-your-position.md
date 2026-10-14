# 04 Play test by ear: your position cue

Status: resolved (2026-10-14)
Type: task
Blocked by: 03

The maintainer plays matches with NVDA and tunes your position cue through the tuning file (`a11y-cues.ini` in the base port's settings folder), offline first; online when a friend is available.

Listen for:

- Whether the cue mixes with the game's music, sounds and the screen reader without masking any of them, and whether the tick is pleasant enough to leave on.
- Pan: whether you can tell where you are across the stage, whether the main stage's edges at 0.7 are where they should be, and whether going past the edge still sounds like moving outward.
- Pitch: whether height reads as height, and whether one octave up and down is too much or too little.
- Pulse speed: whether nearness to the opponent is clear, and whether the slow and fast ends and the far and near distances are right.
- Pause, GAME!, being knocked out and respawning: the cue stops and comes back when it should.
- The tuning file: whether editing it during a match works, and whether its descriptions are clear.
- Clicks, crackle or stepping noise anywhere.

Findings become new issues here, and tuned defaults go back into the code. When it passes, CLAUDE.md's status line says "Play-tested by ear with NVDA", and issue 05 starts.

## Comments

### 2026-10-13, notes for the play test (agent, from issue 03)

- **The file.** `%APPDATA%\melee-pc\a11y-cues.ini`, written with the defaults the first time the game starts with cues on (speech on is enough). The game reads it again about half a second after it is saved, also mid-match, and a line it cannot read is spoken ("Cue settings, line 12 not understood.") and said again when the next match starts. Deleting the file brings every value back.
- **The keys to tune**, all under the "Your position cue" heading:
  - `your_position.on`, `your_position.volume` (0 to 100%) and `your_position.boost` (1 to 8 times, default 1.5): how loud the tick sits against the game and NVDA. `cues.volume` and `cues.on` above them move all cues.
  - `your_position.edge_pan` (default 0.7): how far to the side the main floor's ends sound. 1 puts them fully to the side; past the ends the pan goes on to fully left or right at the side blast zones.
  - `your_position.up_range` (0 to 2 octaves, default 1) and `your_position.down_range` (0 to 1 octave, default 1): the pitch at the top of the camera's view and at the bottom blast zone, from the main floor. Down stops at 1 octave because the tick would overlap itself any lower at the fastest pulse; say if you want more, and the tick gets shorter first.
  - `your_position.slow_interval` (default 0.8 s) and `your_position.fast_interval` (0.07 s and up, default 0.08 s): the pulse with the opponent far and near.
  - `your_position.far_distance` (default 120 units) and `your_position.near_distance` (default 10 units): where slow and fast begin. Final Destination's main floor is 171 units wide and Battlefield's 137; a fighter is 10 to 20 units tall.
- **When it sounds.** From GO; in Training at once. Silent while paused (VS pause and Training's menu alike), on GAME!, and from being knocked out until the respawn platform appears. Training's slower speeds keep it going. A pause stops it and unpausing starts it with a tick at once.
- **What the drive runs could not reach, so listen for it:**
  - Whether it can be heard at all against the game's music and sounds, and how it sits beside NVDA: agent runs are silent, and the dump only shows the cue's own level (about -31 dB average, -11 dB peak with one voice at the defaults).
  - Whether pan and pitch feel right while fighting, not just walking: the runs walked, jumped and fell.
  - Yoshi's Story: the cue measures from its main floor at y = 0 and its ends at ±56 where the slopes finish. Randall the cloud is left out, so standing on it below the stage sounds below the floor.
  - Stages outside the six tournament ones: the frame is found the same way but was not checked. On Princess Peach's Castle the floor found across the middle is the raised roof, so its lower sides sound below the floor.
  - Online with a friend: the agent runs showed each side's cue following its own player.
  - Venom crashes the game as its match starts (a base port bug, cues on or off), so avoid it, and Random, for now.

### 2026-10-13, review of issues 01 to 03 (agent): for the maintainer to decide

The review fixed what was plainly wrong (commit "A11y: match cues, review fixes"). These need your judgement, most important first:

- **Tuning gives no spoken feedback for a clamped value or an applied save.** `your_position.down_range = 2` silently becomes 1, `boost = 20` silently 8; only the log says so. A save that applies is silent too, which is hard to tell from one that did not take when the change is subtle. Options: speak clamped lines in the same one announcement as unreadable ones ("Cue settings, line 14: 2 is past the most, 1."); also queue a short "Cue settings applied." on a reload in a match; or leave both until this play test says they are missed. Recommendation: speak the clamps; decide the confirmation by ear here.
- **The settings model's class names clash with CONTEXT.md.** The code calls it `Setting`, `Settings` and `SettingsFile`; CONTEXT.md keeps "setting" for the port's options and says "values" for cue settings. Options: rename the classes (`Value`, `ValueModel`, `ValueFile`) before the port menu builds on them, or reword the glossary. Recommendation: rename, before issue 05 adds more users.
- **hooks.cpp now runs the cue system's whole lifetime**: the engine and its output, three settings objects built in order, the settings path, polling and the match-start repeat. Options: a `CueSystem` (open, scene entered, tick, master volume) with hooks.cpp forwarding, or leave it. Recommendation: extract it with issue 05, when the second cue joins.
- **A netplay stall keeps the tick pulsing.** While the game waits for a stalled peer no tick runs, so your position cue keeps pulsing at its last place until the session times out. Options: the engine fades a voice whose targets are older than about a quarter second, or leave it. Recommendation: listen for it in the online play test.
- **`tools/smoke_test.py` reaches the default cue settings file** unless the variable is set: its launcher cases leave speech, and so cues, on. The `verify` skill now says to set `MELEE_A11Y_CUE_SETTINGS` for it; making the script set it would be a new edit to a base port file. Recommendation: leave the script alone.

### 2026-10-14: play-tested by ear

The maintainer played matches with NVDA, mostly on Battlefield and Final Destination, and the basics work: the tick is audible beside the game and NVDA, with no clicks, crackle or other sound problems. No new issues.

- **New defaults from the tuning file:** `your_position.boost` 2 (was 1.5) and `your_position.edge_pan` 0.6 (was 0.7), now in the code. A tuning file already written keeps its own values; deleting it brings the new defaults.
- **Princess Peach's Castle:** a short try where the pan felt a bit odd, possibly the raised roof taken as the main floor (see the notes above). It needs more listening before it becomes an issue. The maintainer may later drop the stage-based frame altogether, depending on how the coming cues sound.
- **How the cues sound together** is left to the later play tests (06, 08, 10).
- **The points left by the review** were not raised. Their recommendations stand: rename the settings classes and extract a `CueSystem` when issue 05 adds the second cue, and listen for the netplay stall online. Online was not tested here; it joins issue 06's play test, which already plans one with a friend.
