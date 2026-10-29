# 03 The New challenger line

Status: ready-for-agent
Type: task
Blocked by: 02

Spec: `.scratch/unlock-notices/spec.md`, "What the New challenger screen says".

## Machinery

- **The challenger data**: `game_access.c` reads the challenger's `CharacterKind` and the challenged port from the data `gm_InitChallengerData` fills (`gm_1736.c`). If it cannot be read cleanly, a hook in `gm_Scene_Approach_OnEnter` passes them instead.
- **Two or more humans**: find the cleanest source for whether the match that brought the challenger had two or more humans, and the challenged player's label (name tag or port, as the match announcements make it).
- **The line**: `scene_speech.cpp`'s `GS_APPROACH` entry says "Warning! Challenger approaching! A new foe has appeared: Jigglypuff. A to fight.", with the player label before the key when two or more humans played. The name comes from the fork's character names. An unknown character is left out of the line and logged once.

## Done when

- `tools/a11y/challenger.drive` waits on the new line, for Jigglypuff and for Mewtwo, and passes.
- The unit tests the spec lists for the New challenger line pass.
- A run with two humans (controller 1 and the drive's second port, if the driver can reach one; otherwise a unit test alone) says the label.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.
- `CLAUDE.md` has one "Accessibility status" line for unlock notices and the New challenger screen, naming the play test as waiting (issue 04).

## Comments
