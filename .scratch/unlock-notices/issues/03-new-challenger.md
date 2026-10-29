# 03 The New challenger line

Status: resolved (2026-10-29)
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

### 2026-10-29, implementation (agent)

Implemented as the spec has it, from the scene hook with no hook of its own: `a11y_game_challenger` in `game_access.c` reads the challenger data, `UnlockSpeech::challenger` (`unlock_speech.cpp`) says the line, `hooks.cpp`'s scene announcement hands `GS_APPROACH` to it, and the scene table leaves the scene silent.

- **The challenger data reads cleanly.** `gm_GetChallengerData()` holds the challenger (`cpu_ckind`), the challenged human's port (`human_slot`, the pad the screen reads) and name tag slot, filled before the screen and kept until the next challenger, so no hook in `gm_Scene_Approach_OnEnter` was needed.
- **Two or more humans.** The challenger data does not say. The cleanest source is the mode: a single-player mode (each caller of `gm_InitChallengerData` in `gmevent.c`, `gmgover.c`, `gmmultiman.c` and `gm_17C0.c`) hands over to `GM_CHALLENGER_APPROACH` with its one player, so the count is 1 there; VS and the special melees choose the challenged human from the standings of the match just ended (`gmVsMelee_ExitResults`, `findSmallestLoser`), which stay in `gmVsMelee_VsExitInfo.match_end`, and the humans are counted there. The label is the match announcements': the name tag where there is one, otherwise "P" and the port.
- **The line.** "Warning! Challenger approaching! A new foe has appeared: Jigglypuff. A to fight.", "… Jigglypuff. P2, A to fight." with two or more humans. A kind with no name says the screen's own "A new foe has appeared!" in its place, logged once. The spec put the line in `scene_speech.cpp`'s `GS_APPROACH` entry; the table holds fixed words, so the entry is now silent with a comment, and the unlock notice reader, which already serves the feature, says the line in its place, as LAN play's lobby takes the online lobby's.
- **Seen in runs.** `challenger.drive` now waits on the line for any named challenger: Jigglypuff's save said "Warning! Challenger approaching! A new foe has appeared: Jigglypuff. A to fight.", Mewtwo's "… Mewtwo. A to fight.", with no "no name" line. `run_level.py` fails the menus level's run unless Jigglypuff's line comes once.
- **Two humans.** No driven run: `drive.py` presses port 1 only, and the event pipe's gamepad steps reach the port menu and the launcher, not the game's pads. The unit tests cover the label (the port, a name tag, and a single-player mode's one human with a tag left unlabelled), as the issue allows.
- **Tests.** `unlock_speech`: the challenger named with the key, two humans with the port and with a name tag, a kind with no name left out and logged once. `scene_speech`: the scene silent.
- **Docs.** `docs/a11y/native-menus.md`: "The New challenger screen" and "Where the fork reads the challenger" after "Challengers and their thresholds", the section's opening and the 1P-mode inventory's "New Challenger" pointing there. The `verify` skill: the line, the run's count, the unit tests and a two-human match as out of reach. `CLAUDE.md`: the feature's "Accessibility status" line, the play test waiting.
- **Verification.** Menus level: build with the "Rollback sections" line, style, unit tests (all but `launcher_data`, known), `run_level.py menus` 32 of 33 with `training_menu` on its known number count (10); its first rerun lost a press on the way to Event Match ("no pad change seen"), its second passed, unchanged.
- **Left open.** The play test (issue 04): whether the line is clear over the approach music, and "A to fight" said at once though A works only after 3 seconds. A two-human New challenger screen is heard first there, if at all.

