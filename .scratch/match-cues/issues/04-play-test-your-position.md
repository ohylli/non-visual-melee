# 04 Play test by ear: your position cue

Status: ready-for-human
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
