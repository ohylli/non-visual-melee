# Character select: speech and steering

Status: agreed 2026-09-29 (design conversation with the maintainer)

The maintainer chose the approach (the fork steers the hand through the controller), D-pad stepping as the way to move, the stick staying as it is with hover announcements, and quick jumps on L, R and Z as a later step. Their answers to the draft's questions are recorded under "Answered questions". The wording of announcements is a first proposal for the play tests to judge.

## Goal

A blind player can use character select alone, offline and online: hear where the hand is, move between targets with the D-pad as in a traditional menu, choose a character, set up CPU opponents, and start the match. Play stays compatible with base port builds online, and a sighted helper can still use the stick as usual and be heard doing it.

How the game's menus run is in the primer `docs/a11y/native-menus.md`. Vocabulary is the glossary in `CONTEXT.md`, plus the new terms below.

## New terms

Proposed for `CONTEXT.md` once this spec is agreed.

- **Hand**: a player's cursor on character select. The decomp calls it a cursor.
- **Coin**: the token a hand carries onto a portrait to choose a character.
- **Portrait**: one character's picture on character select.
- **Player slot**: one of the four panels along the bottom (the decomp's door).
- **Target**: anything on a free-cursor screen that reacts to the hand: a portrait, a button, a slider.
- **Steering**: the fork moving the hand by generating stick input, as opposed to changing the hand's position in game memory.
- **Step**: one D-pad press, moving the hand to the neighbouring target.
- **Glide**: the hand's movement to a target under steering.

## Scope

In:

- **Character select in VS modes**, offline and online: portraits, the coin, the player slots' HMN/CPU button, the team button, the CPU level and handicap sliders, the top bar (Teams button, rules header, Back), Ready to Fight.
- **Character select in single-player modes**: the same portraits with one hand, plus the stock and difficulty arrows.
- **Hover announcements** for the local player's hand, whatever moves it.
- **Steering** with the D-pad.
- **Other players' choices** announced, online only.
- **Stage select, one sentence**: its scene announcement tells the player that Start picks a random stage, so a match can be reached before stage select has its own feature.

Out, noted for later:

- Quick jumps on L, R and Z.
- The name tag window and name entry.
- The rules screen and item switch opened from the rules header. They say their name and "No speech yet." as leaf screens do.
- Reading the rules header's text ("2-minute KO fest!").
- Stage select proper. It reuses the steering built here.
- Costume names. A costume is announced by number.
- More than one blind player on one machine.
- Saying why Start was refused.
- PAL discs and the Japanese language setting.

## What the game gives us

Facts the decisions rest on, from reading the code on 2026-09-29. Function names with addresses in them are the decomp's; their meaning here is inferred from the code.

### The screen

As drawn (`docs/screenshots/character-select.png`, a save with locked characters):

- Top left the mode's title ("MELEE VS"), top centre the rules header ("2-minute KO fest!"), top right "BACK".
- Three rows of portraits, each with the character's name drawn under the picture in capitals: "Dr.MARIO", "D K", "C.FALCON", "ICE CLIMBERS". A locked character is a "?" or is not drawn at all.
- Four player slots along the bottom. Each has a tab reading "HMN", "CPU" or "N/A", a name box, and for a CPU a slider labelled "CPU Level" with its number. The name box of a CPU shows the character's name as game text.
- The hand is a white glove carrying a badge such as "P1".

### State is private to one file

Every variable of the screen is `static` in `src/melee/mn/mncharsel.c`: the hands, the coins, the player slots, the portrait table, the mode and the flags. Unlike the main menu tree, nothing can be read from outside the file. Some of the types are in `src/melee/mn/types.h` (`CSSData`, `CSSDoor`, `CSSDoorsData`, `CSSIcon`); the hand's and the coin's structs are defined inside `mncharsel.c`.

### The hand

- One hand per port in VS modes, one hand in single-player modes. Each is updated once a simulated frame by `mnCharSel_CursorThink`.
- Position is in the screen's own units. The hand is kept inside x -35 to 26 and y -22 to 25.
- Three bands by height: the top bar above y 22, the portraits between 0.2 and 22, the player slots below 0.2.
- The hand starts over its own player slot, below every button.
- Movement per frame is `0.0002 * (x² + y² - 200)` in the stick's direction, where x and y are the stick values the game sees. Below a squared length of 200 (a tilt of about 14) nothing moves. Speed grows with the square of the tilt: a tilt of 20 moves 0.04 units a frame, 40 moves 0.28, 80 (a full push) moves 1.24.
- A controller that is unplugged hides its hand and turns its player slot into a CPU.

### The coin

- A hand in the portrait band whose player has no character gets its coin into the hand automatically.
- A carried coin sits at the hand's position plus (2.7, -2.0). Hovering is tested at the coin, not at the hand.
- The coin over a portrait sets that player slot's hovered portrait and plays the move sound when the portrait changes. Locked portraits are never hovered.
- A on a portrait chooses: the coin is dropped there, the chime plays and the game's announcer says the character's name.
- A over no portrait plays the refused sound.
- B while carrying puts the coin back on the character chosen before, if there was one.
- Carrying one's own coin down into the player slot band returns the coin and clears the character.
- A free hand over a portrait hovers nothing. A near a resting coin picks it up, if the coin is one's own or a CPU's. B anywhere in the portrait band calls one's own coin back into the hand.
- A hand carrying a coin cannot press any button; the top bar does not react to it.
- With all 25 characters unlocked, the two empty corners of the bottom row are hidden targets that choose a random character.

### Targets

| Target | Where | Reacts to |
| --- | --- | --- |
| 25 portraits | a table of rectangles, 7 units wide and 7 high; rows of 9, 9 and 7 | a carried coin |
| HMN/CPU button, one per player slot | a rectangle, about 6 by 5 units | free hand, A: human, CPU, closed, human |
| Team button, one per player slot | a rectangle below the HMN/CPU button, teams only | free hand, A: next of three teams |
| CPU level slider | the knob's position, read from the model at runtime; grab within about 2.2 units | free hand, A grabs, A releases |
| Handicap slider | as the CPU level slider, when the handicap rule is on | the same |
| Name box | read from the model at runtime | free hand, A opens the name tag window |
| Teams button | top bar, left of x -25.5 | free hand, A |
| Rules header | top bar, x -17 to 15; not in Stamina mode | free hand, A opens the rules screen |
| Back | top bar, right of x 17.3 | free hand, A; holding B for half a second does the same from anywhere |
| Stock arrows, difficulty arrows | single-player modes only | free hand, A |

- If Luigi is locked, Luigi's and Pikachu's portraits trade places.
- A held slider follows the hand's x. One level is 1.25 units wide; levels run from 1 to 9.
- A click on the HMN/CPU button moves the hand to a fixed height on the button.
- A new CPU that never had a character gets a random one.

### Buttons that need no aim

- X and Y change the costume of one's own character, or of the coin being carried.
- Start leaves for the next screen when Ready to Fight is shown, and plays the refused sound otherwise.
- Ready to Fight shows when every open player slot has a character, at least two players are in (two teams in a team match), no hand carries anything and no name tag window is open.
- L+R+Start returns to the main menu.

### Input

- Character select reads A, B, X, Y, Start and the stick of each port from `HSD_PadCopyStatus`. It never reads the D-pad, Z, the C-stick, or L and R on their own.
- The screens opened from it (rules, name entry) are digital menus that do read directions.
- Port 1's input passes through `publish_locked` in `src/pc/keyboard.c`, the only writer of that port's virtual pad. It merges the keyboard, the scripted key driver and a GameCube adapter, and publishes once per video frame.
- `PADRead` then merges the virtual pad with an SDL gamepad on the same port: buttons are combined, and each stick axis takes whichever value is further from rest.
- Between `PADRead` and the stick value the screen reads lies the game's own clamping (`HSD_PadClamp`). A full keyboard push of 80 arrives as 80. What smaller values arrive as is still to be measured.

### Online

- From the moment two peers connect, every frame of every scene runs on synced input. Character select is the unchanged native screen, and the other player's hand is moved by their controller data.
- Menus run in lockstep: a frame is simulated only when both inputs for it have arrived. Rollback is armed only inside a match, so `pc_net_resim()` is false here.
- Local input is sampled with `PADRead` once per fresh simulated frame (`capture_local_sample` in `src/pc/net.c`), after the virtual pad merge, and takes effect a number of frames later: the input delay, 2 by default, more on a slow link.
- Simulated frames and video frames are not one to one. A time-sync correction simulates an extra frame on the last sample, and a stall simulates none.
- The local player's controller is always physical port 1. Which player they are in the game is `pc_net_local_player()`.
- The peer receives only controller data. Nothing about characters or costumes is sent separately.
- In a menu, the desync check covers controller data and the random seed only. A difference in menu state would go unnoticed until the match, where it breaks the session.
- Stick values of 2 or less are sent as 0.

## Decisions

### Two stages

1. **Speech.** The fork reads the screen and speaks. It writes nothing. This stage is useful by itself to a blind player with a sighted helper, and it is what stage 2 is checked with.
2. **Steering.** The D-pad moves the hand between targets.

### The rule that keeps online play compatible

The fork never writes to the screen's state. It changes only what the local controller appears to do, before the game and the netplay code sample it. The peer cannot tell a steered hand from a hand moved by a player.

Consequences:

- The steering logic does not have to behave the same on two machines, or in two versions of the fork. Only its output travels.
- A fork build against a base port build, and two fork builds against each other, are the same case.
- D-pad presses reach the game and the peer too. Character select ignores them.

This narrows the primer's "Read, don't steer": feeding input is safe where it enters as controller input, and unsafe anywhere later.

### How the fork attaches

Three new hooks. Names are suggestions.

| Hook | Where | Why |
| --- | --- | --- |
| `pc_a11y_css_frame` | `mnCharSel_Scene_OnFrame` in `mncharsel.c` | Hands the fork the screen's private state once a frame: the screen's data, the player slots, the portrait table and the flags. The fork compares with the previous frame. |
| `pc_a11y_css_hand` | the end of `mnCharSel_CursorThink` | One hand's port, state, held thing and position, as plain numbers. |
| `pc_a11y_pad` | `publish_locked` in `src/pc/keyboard.c`, before the pad is published | Lets the fork add its stick value to port 1's pad. Stage 2 only. |

- The first two hooks pass state out because the fork cannot reach it. The hand hook passes numbers instead of a pointer, so the fork does not have to copy a struct that exists only inside `mncharsel.c`.
- The screen is polled. It has far too many transitions for a hook each; this is the case the isolation rule allows polling for.
- The pad hook is the fork's first hook that changes anything. It is one line in a port layer file.
- Reads of game memory stay in `game_access.c`.
- Every hook returns at once while `pc_net_resim()` is true.
- The fork keeps its own copy of nothing it can read: portrait rectangles and button positions come from the game's tables through the frame hook, so the Luigi and Pikachu swap and locked portraits need no special handling.

### Whose hand

- The fork follows one hand, the local player's.
- In VS modes offline it is the hand of port 1. Steering can only reach port 1, since that is the virtual pad the port layer offers.
- Online it is the hand of `pc_net_local_player()`.
- In single-player modes it is the one hand.
- Steering obeys only the D-pad of that hand's controller.

### Stage 1: announcements

The local hand's announcements interrupt. Announcements about other players are queued behind current speech.

| Moment | Announcement | Example |
| --- | --- | --- |
| The screen opens | the screen, the player, their state | "Character select. Player 1, no character." |
| A carried coin enters a portrait | the character | "Fox" |
| A carried coin leaves every portrait | nothing | |
| A character is chosen | nothing: the game's announcer says the name | |
| The hand picks up a coin, or the coin jumps into it | whose coin | "Holding your coin", "Holding player 2's coin" |
| The coin goes back and the character is cleared | | "No character" |
| B puts the coin back on the earlier choice | | "Back to Fox" |
| A free hand enters a button | the button and its value | "Player 2: CPU", "Teams: off", "Rules", "Back" |
| A free hand reaches a slider's knob | the slider and its value | "Player 2 CPU level: 1" |
| A player slot changes kind | the slot, its kind, its character | "Player 2: CPU, Yoshi" |
| A held slider's value changes | the value | "Level 4" |
| A slider is grabbed or released | | "Holding the slider", "Released" |
| The costume changes | its number | "Costume 2" |
| The team changes | the team as drawn | "Red team" |
| Ready to Fight appears | | "Ready to fight. Press Start." |
| Ready to Fight disappears | nothing | |
| Online, the other player chooses a character | the player and the character | "Player 2: Fox" |
| Online, the other player's slot changes | as for one's own | "Player 2: closed" |

- **A free hand moved over the portraits by the stick is silent.** A does nothing there without a coin. Stepping is different: a step always names where it goes (see "Stage 2: stepping").
- **A choice is silent.** The game's announcer names the character, in its own voice, which tells a choice from a hover.
- **What the local hand does is always announced**, whichever player slot it acts on: setting up a CPU is the local player's own action.
- **What other players do is announced online only.** Offline, the others are in the same room. Online, a sighted player sees the opponent's choice, so the blind player hears it. The game's announcer names a character for every player, so speech adds who chose.
- Another player's hovering is never announced.
- A missing name is spoken as "Unknown character 27" and logged, as on the main menu tree.

### Where the words come from

- **Characters**: a fork string table keyed by the portrait's character (`CharacterKind`), holding the names as drawn under the portraits, so the blind player hears what the sighted player reads. Capitals are written in normal capitalisation, as on the main menu tree: "Dr. Mario", "DK", "C. Falcon", "Ice Climbers". The names of characters locked in the screenshot are checked against a run with everything unlocked.
- **Player slot kinds**: "human", "CPU" and "closed" for the screen's "HMN", "CPU" and "N/A". These three are spoken as words, not as drawn: "HMN" and "N/A" read badly.
- **Players**: "Player 1" to "Player 4", for the screen's "P1".
- **Buttons**: "Teams", "Rules", "Back", "CPU level", "Handicap".

### Stage 2: stepping

The targets form rows, top to bottom:

1. The top bar: Teams, Rules, Back.
2. The three portrait rows.
3. The player slots: for each slot its HMN/CPU button, then its team button, its CPU level slider and its handicap slider where they exist.

Rules:

- **Left and Right** move to the next target in the row. **Up and Down** move to the row above or below, to the target nearest in x.
- **Targets that do not exist are skipped**: locked portraits, the team button outside team matches, the slider of a slot that is not a CPU, the Teams button in modes without teams.
- **No wrapping.** A press at an edge moves nothing and repeats the current target's name, so no press is silent.
- **From anywhere else** (the start position, or wherever a helper's stick left the hand), a press moves to the nearest target in that direction.
- **The announcement comes at the press**, not on arrival, so steps can follow each other quickly. A press during a glide changes the glide's destination.
- **A step always names its destination**, a portrait too when the hand is free, since the player crosses the portraits on the way between the player slots and the top bar and must know where they are.
- **While a glide runs, hover announcements are held back**, so the portraits the hand crosses are not read out. Arriving where the announcement said is silent. Arriving anywhere else is announced as usual.
- **A glide that fails** is announced ("Could not reach Fox") and logged.
- **The player presses every button themselves.** The fork generates stick movement and nothing else.
- **On a portrait**, the glide aims so that the coin's position is at the portrait's centre, with or without a coin in the hand.
- **A resting coin the hand may pick up** (a CPU's, or one's own) is named with its portrait when a free hand steps there: "Yoshi, player 2's coin". The glide then aims at the point from which A picks that coin up, not at the portrait's centre, since a coin can rest anywhere inside its portrait.
- **With a slider held**, Left and Right move one level.
- **Carrying a coin down** into the player slots clears the character, as the game does for any hand. The announcement says so.

### Stage 2: the glide

Steering decides once per simulated frame, in the hand hook, from what the game shows: the hand's position and the stick value the game applied that frame. It does not count video frames, since the two differ online.

Requirements, for the implementer to meet as they see fit:

- The hand comes to rest inside the target, far enough from its edge that a frame more or less of movement would not have missed.
- It works with an input delay of 0 to 10 frames, with a repeated sample and with a skipped one.
- The longest glide, corner to corner, takes under 1.5 seconds at no delay.
- A slow final correction is available, since a small tilt moves the hand by hundredths of a unit.

A simple shape that meets them: push at full tilt until the pushes already asked for cover the distance left, let the hand come to rest, measure, correct with a short slow push if needed.

### Stage 2: safety

- **Steering runs on character select only.** It is off in every other scene, and on character select while the rules screen, name entry or the name tag window is open, and once the screen has begun to leave.
- **The stick returns to rest by itself.** A requested stick value is published once, then dropped unless the next simulated frame asks again. A scene change, a paused game or the port menu opening cannot leave the stick tilted.
- **The player's stick wins.** The pad merge already gives way to a stick pushed further than the fork's. When the stick the game applied is not what the fork asked for, the glide is abandoned.
- **Steering follows the accessibility switch.** With accessibility off, the D-pad does nothing, as in the base port.
- **Agent runs** have speech off, so a developer switch (`MELEE_A11Y_STEER=1`) turns steering on by itself. `drive.py` sets it.
- **Every glide is logged** in the speech log: its destination, and on arrival the frames it took.

### Stage select, one sentence

The scene announcement for stage select becomes "Stage select. No speech yet. Press Start for a random stage." On entering, nothing is hovered, and Start with nothing hovered picks a random stage. To be confirmed by a drive run before the wording lands.

### Names and layout

Suggestions; the implementer may reshape them.

| File | Role |
| --- | --- |
| `css_speech.hpp/.cpp` | The reader: keeps the previous snapshot, compares, composes announcements. Pure. |
| `css_names.hpp/.cpp` | The string table: characters, slot kinds, buttons. |
| `css_targets.hpp/.cpp` | Builds the rows of targets from a snapshot, and answers "what is at this position" and "what is next in this direction". Pure. |
| `steering.hpp/.cpp` | The glide: position, destination and applied stick in, stick value out. Pure, and not specific to character select. |
| `game_access.h/.c` | Filling the snapshot from what the hooks hand over. |

### Tests

- **Unit tests of the reader**, fed snapshots, checked through the fake bridge. They cover every row of the announcement table, including the rows that say nothing.
- **Unit tests of the targets**: stepping from every target in every direction, with locked portraits, with Luigi locked, in a team match, in a single-player mode.
- **Unit tests of the glide**: a simulated hand using the game's formula, inputs delayed by 0 to 10 frames, with a repeated and a skipped sample. From every target to every other, the hand ends inside the target.
- **Drive scripts**, committed under `tools/a11y/`. One moves the hand with the stick and waits on hover announcements. One steps with the D-pad through every row, chooses a character, sets up a CPU and its level, and starts the match.
- **Two instances on one machine.** Both connect over loopback (`MELEE_NET`, `MELEE_NET_PORT`, `MELEE_NET_PLAYER`, `MELEE_NET_KEY`), with the input delay pinned by `MELEE_NET_DELAY` or a slow link simulated by `MELEE_NET_SIM_DELAY_MS`. One instance steps and chooses. The run passes when:
  - the stepping instance logs the expected announcement after each press;
  - the other instance announces the same choice as another player's;
  - neither logs `net: DESYNC`;
  - the two recordings of controller data (`MELEE_NET_RECORD`) match.
- **Fork against base port**: the same run with the second instance built from `upstream/master`. The evidence there is no desync and both reaching the match.
- **Play test.** The maintainer, by ear, after each stage. Only what that confirms is called working.

The base port's own two-instance harness, `tools/net_test.py`, runs on Linux only, and base port online play is unverified on Windows. The first step of the two-instance work is a smoke check that a loopback session comes up on Windows at all. If it does not, the unit tests of the glide stand in until it does.

### Docs

- **ADR-0004**: free-cursor screens are steered through the controller. Rejected: guidance only, shaping the player's stick input, writing the screen's state or replacing the screen, a list of targets to choose from with a confirm button.
- **`CONTEXT.md`**: the new terms above.
- **The primer** `docs/a11y/native-menus.md`:
  - "Read, don't steer" is reworded as above.
  - Picking up a coin sets the slot's hovered portrait to `0xD`, not `0x19`; `0x19` is written only when the coin returns to the slot.
  - Added: hovering is tested at the coin, B calls the coin back, the coin jumps into a hand with no character, the hidden random corners, the single-player arrows, an unplugged controller turning its slot into a CPU, and that all of the screen's state is private to its file.
- **CLAUDE.md**: one "Accessibility status" line per stage; the drive scripts and the two-instance run under "Verification"; the pad hook under "Online compatibility" as the one place the fork writes.

## Answered questions

The maintainer's answers to the draft, 2026-09-29. The sections above already follow them.

1. **Wrapping**: none.
2. **Choosing**: silent; the game's announcer is enough.
3. **Slot kinds**: "closed" for the screen's "N/A".
4. **Character names**: as drawn, matching what the sighted player reads.
5. **Other players' choices**: online only.
6. **A free hand over portraits**: silent. The spec reads this as being about the stick; a step still names its destination. To be confirmed with the maintainer.
7. **Single-player modes**: in this feature.

## Open questions

For the implementer to find out:

1. Whether a loopback session between two instances works on Windows.
2. Whether a direct session (`MELEE_NET`) walks the ordinary menus to character select in sync, or the LAN lobby is the only route.
3. What stick values below a full push arrive as after the game's clamping.
4. Whether `mnCharSel_Scene_OnFrame` and the hand's update keep running while the rules screen or name entry is open.
5. Whether `publish_locked` runs on the same thread as the simulation. If not, the requested stick value crosses threads.

## Issues

1. `issues/01-portraits-and-coin.md`: the two reading hooks, the snapshot, and announcements for portraits and the coin.
2. `issues/02-player-slots-and-top-bar.md`: announcements for the player slots, the top bar, Ready to Fight and the other player online. Blocked by 01.
3. `issues/03-play-test-speech.md`: the maintainer, by ear. Blocked by 02.
4. `issues/04-glide-and-portrait-steps.md`: the glide as pure code, the pad hook, stepping over the portraits. Blocked by 01.
5. `issues/05-steps-everywhere.md`: stepping over the player slots and the top bar, and the sliders. Blocked by 02 and 04.
6. `issues/06-two-instances.md`: two instances on Windows, runs at several delays, fork against base port. Blocked by 04.
7. `issues/07-single-player-modes.md`: one hand, the stock and difficulty arrows. Blocked by 05.
8. `issues/08-play-test-steering.md`: the maintainer, by ear. Blocked by 05 and 07.

Docs are part of the issue they belong to, not an issue of their own.
