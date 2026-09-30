# 02 Player slots, the top bar and Ready to Fight speak

Status: resolved (2026-09-30)
Type: task
Blocked by: 01

The rest of stage 1 of `.scratch/character-select/spec.md`: every target outside the portraits, the screen's state changes, and the other player online. Read the spec first; issue 01 built the hooks, the snapshot and the reader this extends.

## Machinery

- **Snapshot grows**: per player slot its team, CPU level, handicap, and whether a slider of it is held; the teams rule; the match type; the Ready to Fight flag (`mnCharSel_804D6CF7`); whether a name tag window is open; the rectangles of the HMN/CPU and team buttons; the positions of the slider knobs and name boxes, which the game reads from its models at runtime.
- **Extend the frame hook's arguments** if the snapshot needs more of the file's private state. It stays one call.
- **What is under the hand** is decided by the fork from the same rectangles and distances the game uses, since the game keeps no "hovered button" anywhere. The spec's target table has them. Keep this in `css_targets.cpp`, pure, so stage 2 can use it to build its rows.
- **Who acted.** A change to a player slot is the local hand's when the hand is on that slot's button, or holds its coin or slider. Those are always announced. Any other change is another player's and is announced only when `pc_net_active()`.

## Speech in this slice

Wording and rules are in the spec, section "Stage 1: announcements". This slice covers:

- A free hand entering a button: "Player 2: CPU", "Teams: off", "Rules", "Back".
- A free hand reaching a slider's knob: "Player 2 CPU level: 1".
- A player slot changing kind: "Player 2: CPU, Yoshi", "Player 3: closed".
- A slider grabbed, its value changing, released: "Holding the slider", "Level 4", "Released".
- The team changing: the team as drawn.
- The hand picking up a CPU's coin: "Holding player 2's coin".
- Ready to Fight appearing: "Ready to fight. Press Start." Disappearing: nothing.
- Online, the other player choosing a character or changing their slot, queued behind current speech: "Player 2: Fox".
- The rules screen and name entry opened from here: their name and "No speech yet.", as leaf screens say it. Coming back from them: the opening announcement again.

## Done when

- `cmake --build build` passes, and the unit tests pass, extended to cover:
  - each kind of button entered and left;
  - the three kinds of a player slot in turn, including the random character a new CPU gets;
  - a slider grabbed, moved over several levels, released;
  - a team match: the Teams button, a team button;
  - Ready to Fight appearing and disappearing;
  - another player's choice and slot change, offline (silent) and online (queued);
  - the local hand choosing a character for a CPU (silent, the game's announcer speaks).
- Unit tests of `css_targets`: a position inside each kind of target, on an edge, and in empty space; targets that do not exist in the current mode.
- The drive script of issue 01, or a second one, moves the hand down to player slot 2's button, presses A until the slot is a CPU, and waits on each announcement; then picks a character for both and waits on "Ready to fight. Press Start."
- `python tools/check_style.py` passes.
- The primer gains what this slice learned about the buttons and sliders.
- CLAUDE.md: the "Accessibility status" line updated.

## Comments

### 2026-09-30, implementation (agent)

Implemented; hearing it is issue 03.

- **Hook.** `pc_a11y_css_frame` also hands over the name tag windows (`mnCharSel_803F0E8C`), the root of the screen's model (`mnCharSel_804D6CC0`) and the Ready to Fight flag (`mnCharSel_804D6CF7`). Still one call on one line. `game_access.c` takes them as one `A11yCssScreen`.
- **Snapshot.** Per slot: whether any hand carries its coin, whether its own hand holds anything, team, CPU level, handicap, which sliders are held, its name tag window, the HMN/CPU and team buttons' rectangles, both knobs, and the local slot's name box. Per screen: the pending exit as an enum, Ready to Fight, the Teams rule, the handicap rule, which top bar buttons exist, and how many slots the buttons reach (3 in Camera mode). The hand also reports the slider it holds. The knobs and the name box are read from the model the way the game reads them (`lb_80011E24`, `lb_8000B1CC`); `lb_8000B1CC` fills the joint's cached world matrix, which the game's own A presses and every frame's drawing also fill, so it changes nothing the game decides.
- **Targets.** `css_targets.cpp`, pure: `target_at(state, x, y)` repeats the game's tests in its order (the top bar, door by door the HMN/CPU button, team button, CPU level and handicap knobs, then the name box, then the portraits at the coin's position). Real geometry from a run is in `test_css_screen.hpp`, shared by both tests.
- **Wording**, for the play test to judge: "Player 2: CPU", "Player 2 team: red", "Player 2 CPU level: 1", "Player 1 handicap: 9", "Player 1 name tag", "Teams: off", "Rules", "Back"; slot kinds "Player 2: CPU, Pichu", "Player 3: closed", "Player 4: human, no character"; "Blue team" when the hand changes its team; "Holding the slider", "Level 4" (or "Handicap 4"), "Released"; "Ready to fight. Press Start.", joined to whatever caused it in one announcement. Online: "Player 2: human, no character", "Player 2: Fox", "Player 2 team: blue", "Teams: on", queued. The name box is spoken, and A on it says "Name tags. No speech yet.", since the name tag window is out of scope but opens from here.
- **Who acted.** A slot or team change is the local hand's when the hand is on its button (this frame or the last), a choice when the hand held that coin. The local player's own slot opening as their hand first reaches the portraits is silent: "Holding your coin" says it.
- **Rules screen and name entry (the spec's open question 4).** Both open inside the character select scene. `mnCharSel_Scene_OnFrame` and the hands keep running (the pending exit reads 5), but the model and name tag windows are freed, so the snapshot reads neither then. Leaving calls `mnCharSel_802640A0`, which builds the screen afresh with the hands at home and no scene change; the reader treats that return as an arrival and says the opening.
- **Base port crash, fixed in its own commit.** Opening the rules screen from character select crashed the game on Windows, with and without the fork's changes: `mn_80231804` (`mnmainrule.c`) ends its `lbArchive_LoadSections` list with an `int` `0`, which the callee reads back as a 64-bit pointer whose upper half is stack garbage. The terminator is now `NULL`. About 25 other `lbArchive_LoadSections` calls in `src/melee/if/` and `src/melee/mn/` end the same way and work only while that stack slot happens to be zero; left alone.
- **Drive.** `tools/a11y/css_slots.drive` passes: player 1's and 2's buttons, A until player 2 is a CPU with its random character, its CPU level knob, grab, levels, release, the coin and a choice, "Ready to fight. Press Start.", the Teams button on and off (a team match of two red players is not ready), the rules header, the rules screen, and the opening again after B. It does not pick player 2's character by hand: the random one a new CPU gets makes it ready. `css_portraits.drive`, `tree_walk.drive` and `leaf_screens.drive` pass; the bounded `title` and `vs` runs exit cleanly.
- **Unit tests.** `css_targets` covers each kind of target, edges, empty space, the conditions that switch targets off, Camera mode, and single-player modes. `css_speech` adds each kind of button entered and left, the three kinds of a slot in turn, a slider, a handicap slider, a team match, Ready to Fight, another player offline and online, choosing for a CPU, the rules screen and name entry, and the name tag window. `launcher_data` fails as before on Windows. `python tools/check_style.py` passes.
- Not tried in a run: team buttons, the handicap rule, the name box, name entry and online play. Their geometry or state comes from the same reads that the tried ones use.
