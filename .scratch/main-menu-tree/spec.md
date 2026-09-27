# Main menu tree speech

Status: agreed 2026-09-27 (grilling session with the maintainer)

## Goal

The first native menu accessibility feature, sized to prove the approach and give something to hear: a blind player can start the game, get from the opening movie to the main menu, move through every tree screen by ear, change the settings on three leaf screens, and always hear which scene or menu screen they have arrived in, including the ones that do not speak yet.

Vocabulary is the glossary in `CONTEXT.md`: scene, main menu tree, menu screen, tree screen, leaf screen, entry, hovered entry, row, value, description, picture label, game text, string table, announcement, hook. How the game's menus run is in the primer `docs/a11y/native-menus.md`. Why the tree is polled is ADR-0003.

## Scope

In:

- **The 11 tree screens**: Main Menu, 1-P Mode, VS. Mode, Trophies, Options, Data, Regular Match, Stadium, Special Melee, Melee Records, and the base port's Online.
- **Three leaf screens with a reader**: Sound, Screen display, Multi-Man Melee.
- **Every other leaf screen** says its name and "No speech yet."
- **Scenes** are named when the player arrives in them.

Out, noted for later:

- Rules, additional rules, the item and stage switches, name entry, Rumble, Language, Erase data, Event Match and the Data screens: each gets its reader in a later feature.
- Free-cursor screens: character select and stage select.
- Reading memory card prompts, unlock notices and results.
- Anything inside a match.
- New keys: no repeat key, no key to read the description again.
- A switch to turn descriptions off: waits for fork settings.
- PAL discs and the game's Japanese language setting. The tested case is an NTSC-U disc with the game in English.
- Mode-specific scene names such as "Classic, stage 3".

## What the game gives us

Facts the decisions rest on, found by reading the code and by scripted runs on 2026-09-27.

### The tree

- The whole tree is one scene. Its state is the global `mn_804A04F0`: `cur_menu`, `prev_menu`, `hovered_selection`, `confirmed_selection`.
- The state changes on the frame of the press, before the slide animation. Input is accepted again 6 frames later, while the new screen still slides in.
- No entry name, screen title or value word exists as game text. They are picture labels. Each entry's description is game text in `SdMenu`, found through the table `mn_803EB6B0[menu].description_indices[selection]`.
- Lists wrap at both ends on every tree screen.
- Entries that do not exist are skipped by the cursor and not drawn: three removed entries (1-P Mode entry 2, Trophies entry 2, Options entry 3), and All-Star and Sound Test until they are unlocked. The tree never plays the refused sound.
- Any controller drives the tree.
- Coming back from a mode lands on the entry that started it. After an Event Match or a Multi-Man match the scene lands directly on that leaf screen.
- L+R+Start anywhere below the main menu restarts the scene on the main menu.
- The Online screen and the Online entry of VS. Mode have no pictures and no `SdMenu` strings. The base port supplies plain C strings through `mnOnline_Label` and `mnOnline_Description`. The Online screen keeps the "VS. Mode" title picture.
- The game does not rebuild the description for a move made during a slide, so the screen can briefly show the previous entry's description. Speech says the right one.

### The three leaf screens

- **Sound** keeps its row in private state. The two values are readable from outside: the channel through `lbAudioAx_80024BD0()` and the balance through `gmMainLib_8015ED74()`. The balance runs from -100 to +100 in steps of 5. A press that changes nothing plays no sound.
- **Screen display** has one value, changed with A only. It is saved as the deflicker preference, readable through `gmMainLib_8015F4E8()`.
- **Multi-Man Melee** keeps its choice in private state. Left and Right move and wrap. Confirm starts the mode.
- All three, and Language, set their centre text through the shared helper `Menu_InitCenterText` (`src/melee/mn/inlines.h`). The text's number identifies the Sound row (187 or 188) and the Multi-Man Melee choice (171 to 176).

### Scenes

- Every scene is entered through `gm_801A4014` (`src/melee/gm/gm_1A3F.c`). Screens inside a scene are not scene changes.
- A normal launch always plays the opening movie before the title screen. Start or A early in the movie goes to the title screen. Start late in the movie, once the logo is drawn, goes straight to the main menu and never enters the title scene.
- Left idle for 600 frames, the title screen starts the attract loop: a demo fight, the title again, another demo fight, the how-to-play movie, the special movie.
- A scene's kind does not always name what the player would call the screen. The match scene covers VS matches, Classic stages, Target Test and the attract demo. Character select, stage select and results are the same scene in every mode.
- The boot memory card scene passes without showing anything when the save loads, and shows a prompt otherwise. The scene's kind cannot tell which.
- Loading blocks inside the scene's enter function, and the first frame is drawn after it returns.
- The game's announcer already speaks in matches: "Ready", "GO!", "Game!", "Time!".

### What the screens look like

For the maintainer and for wording decisions. Described from screenshots of a run with everything unlocked.

- **Tree screens.** The screen title sits on a tab at the top left; on a nested screen the parent's title comes first, dimmed. The entries are a vertical stack of bars. The hovered entry is a solid yellow bar, the others are black with gold text. To the right is a preview of what Confirm leads to, under the words "NEXT SCREEN". The description is in a box at the bottom centre. No button hints are drawn.
- **Options.** The Language entry is drawn in Japanese, "日本語切り替え" (switch to Japanese), while the game is in English.
- **VS. Mode.** The base port's entry is drawn as "ONLINE" in plain capitals, smaller than the others.
- **Sound.** Title "Sound". Two rows labelled `CHANNEL` and `VOLUME` in small capitals. Channel shows two panels, `STEREO` and `MONO`. Volume is a bar with a divider between `SOUNDS` on the left and `MUSIC` on the right, under an unnumbered scale of 11 tick marks. The hovered row is bright, the other dimmed.
- **Screen display.** Title "Screen Display". One panel labelled `DEFLICKER` with a badge that reads `ON` (green) or `OFF`. It opens as ON. A second panel labelled `SAMPLE` shows a test picture.
- **Multi-Man Melee.** The title tab says "Stadium"; a banner below says "Multi-Man Melee" and "Choose the Mode". Six cards in one row, the hovered one solid yellow.
- **Title screen.** The logo, the words "PRESS START" revolving as a ring of letters, and a copyright block.

`docs/screenshots/main-menu.png` shows the 1-P Mode screen, not the main menu.

## Decisions

### How the fork attaches

Three new hooks, one removed. Names are suggestions.

| Hook | Where | Why |
| --- | --- | --- |
| `pc_a11y_scene_entered` | `gm_801A4014`, after the scene's enter function returns | Names the scene, and tells the menu reader to forget its previous state. |
| `pc_a11y_menu_frame` | `mnMain_Scene_OnFrame` in `mnmain.c` | The poll: compares the tree's state with the previous frame. |
| `pc_a11y_menu_center_text` | `Menu_InitCenterText` in `inlines.h` | Tells the fork which centre text a leaf screen set. |
| `pc_a11y_menu_description` | removed from `mnmain.c` | The fork looks descriptions up itself. |

- The poll is the case the isolation rule allows polling for. ADR-0003 has the alternatives.
- The scene hook passes the scene's kind and the mode's kind. Only the scene's kind is used now.
- Reads of game memory stay in `game_access.c`, the fork's one file that includes the decomp's headers.
- Every hook stays silent while `pc_net_resim()` is true. No menu runs under rollback, so this costs nothing.
- `MELEE_A11Y_TEXT_DUMP=1` keeps working, triggered from the frame hook.

### Where the words come from

- **Screen titles, entry names, row names and value words**: a fork string table keyed by the game's identifiers (`MenuKind` and the selection enums), holding the words as drawn on the English screen, in the screen's own spelling: "1-P Mode", "VS. Mode", "Slo-Mo Melee". Labels drawn in capitals are written in normal capitalisation: "Channel", "Deflicker".
- **Corrections** are made only where the play test finds a word reads badly.
- **The Language entry** is spoken as "Language".
- **Descriptions**: decoded game text, with the lines of a wrapped description joined by a space. A glyph with no known character is left out of the speech and logged.
- **Online**: labels and descriptions are read from `mnOnline_Label` and `mnOnline_Description` at runtime, so they follow base port changes. Capitals are normalised ("DIRECT CONNECT" becomes "Direct connect"), keeping abbreviations such as LAN. The Online screen's title is spoken as "Online", although the screen shows the VS. Mode title.
- **Gaps are audible.** A screen or entry missing from the string table is spoken as "Unknown screen 27" or "Unknown entry 3", and logged.

### The string table

Names as drawn, in cursor order. Hidden entries are left out.

| Menu screen | Entries |
| --- | --- |
| Main Menu | 1-P Mode, VS. Mode, Trophies, Options, Data |
| 1-P Mode | Regular Match, Event Match, Stadium, Training |
| Regular Match | Classic, Adventure, All-Star |
| Stadium | Target Test, Home-Run Contest, Multi-Man Melee |
| VS. Mode | Melee, Tournament Melee, Special Melee, Custom Rules, Name Entry, Online |
| Special Melee | Camera Mode, Stamina Mode, Super Sudden Death, Giant Melee, Tiny Melee, Invisible Melee, Fixed-Camera Mode, Single-Button Mode, Lightning Melee, Slo-Mo Melee |
| Trophies | Gallery, Lottery, Collection |
| Options | Rumble, Sound, Screen Display, Language, Erase Data |
| Data | Snapshots, Archives, Sound Test, Melee Records, Special |
| Melee Records | VS. Records, Bonus Records, Misc. Records |
| Online | from the base port at runtime |

The Melee Records entries and the titles of leaf screens were checked against screenshots on 2026-09-27 (issue 02) and match, except that the random stage switch calls itself "Random Stage".

A leaf screen's name is its title as drawn. Where the title tab shows something else, as on Multi-Man Melee, the name is what the screen calls itself. Every leaf screen's title is the name of the entry that opens it, with these exceptions: "Additional Rules", "Item Switch", "Random Stage". Custom Rules opens Item Switch; Additional Rules opens Random Stage. The Language screen's title is drawn in Japanese like its entry, and is spoken as "Language".

### Tree screen announcements

All interrupt.

| Moment | Announcement | Example |
| --- | --- | --- |
| A tree screen opens | title, hovered entry, description | "Main Menu. 1-P Mode. Solo Smash!" |
| The cursor moves | hovered entry, description | "VS. Mode. Multiplayer battles!" |

- A screen opening is the same announcement by every route: Confirm into it, Back to it, arriving from the title screen, returning from a mode, L+R+Start.
- Going forward repeats the name the player just heard ("Regular Match. Classic. Defeat each foe to advance."). That confirms the press worked.
- Only the screen's own title is spoken, not the dimmed parent.
- A screen change and a cursor move in the same frame give one announcement, the screen's.
- No position ("2 of 5"), no role words, nothing when the list wraps, nothing on Confirm itself.
- Speech happens at the press, not after the slide animation.
- Whoever pressed, the change is announced.

### Leaf screens without a reader

When one opens: its name and the notice, as one announcement. "Rumble. No speech yet."

Nothing else is spoken until the player leaves it, when the tree screen they return to speaks as usual. The notice disappears from a screen when its reader lands.

### Leaf screens with a reader

The opening announcement is: title, row and value, key hint, description. The hint comes once, at the opening only. The description comes last, so any key press cuts it off.

| Moment | Announcement |
| --- | --- |
| Sound opens | "Sound. Channel: Stereo. Left and right to change. Choose between Stereo and Mono sound." |
| Sound, row changes | "Volume: centre. Adjust music and sound effect volume balance." |
| Sound, channel changes | "Mono" |
| Sound, balance changes | "15 toward music", "40 toward sounds", "Centre" |
| Screen display opens | "Screen Display. Deflicker: On. A to change. ON: Display will be smoother and softer. OFF: Display will be sharper and harder." |
| Screen display, value changes | "Off" |
| Multi-Man Melee opens | "Multi-Man Melee. 10-Man Melee. Left and right to choose. How fast can you defeat 10 opponents?" |
| Multi-Man Melee, choice changes | "100-Man Melee. 100 enemies! Can you defeat them all?" |

- A changed value is spoken alone.
- A press that changes nothing speaks nothing: Left on Stereo, Right at the end of the balance.
- The balance is spoken as its distance from the centre, 5 to 100, and the side it leans to, in the screen's own words.
- Multi-Man Melee opens on the choice the game puts the cursor on, which after a match is the mode just played.

### Scene announcements

One announcement, interrupting, once the scene has loaded. Keyed by the scene's kind only. The wording is a first proposal for the play test to judge.

Scenes to operate get their name and the notice:

| Scene kind | Announcement |
| --- | --- |
| `GS_CSS` | "Character select. No speech yet." |
| `GS_SSS` | "Stage select. No speech yet." |
| `GS_RESULTS` | "Results. No speech yet." |
| `GS_TOY_GALLERY` | "Trophy gallery. No speech yet." |
| `GS_TOY_LOTTERY` | "Trophy lottery. No speech yet." |
| `GS_TOY_COLLECTION` | "Trophy collection. No speech yet." |
| `GS_TOU_SETUP` | "Tournament setup. No speech yet." |
| `GS_TOU_BRACKET`, `GS_TOU_ALT` | "Tournament bracket. No speech yet." |
| `GS_ONLINE_LOBBY` | "Online lobby. No speech yet." |
| `GS_PRIZE_INTERFACE` | "Unlock notice. No speech yet." |
| `GS_GAMEOVER` | "Continue screen. No speech yet." |
| `GS_CAMERA_VS` | "Camera Mode notice. No speech yet." |

Things to watch get their name, and the way out where there is one:

| Scene kind | Announcement |
| --- | --- |
| `GS_MOVIE_OPENING` | "Opening movie. Press Start to skip." |
| `GS_TITLE` | "Title screen. Press Start." |
| `GS_MOVIE_HOWTO` | "How to play movie" |
| `GS_MOVIE_OMAKE15` | "Special movie" |
| `GS_MOVIE_END` | "Ending movie" |
| `GS_APPROACH` | "New challenger" |
| `GS_INTRO_EASY`, `GS_INTRO_NORMAL` | "Stage intro" |
| the `GS_CUTSCENE_*` kinds | "Cutscene" |
| `GS_REGEND_TOYFALL` | "Trophy award" |
| `GS_REGEND_CONGRATS` | "Congratulations" |
| `GS_STAFFROLL` | "Credits" |

Silent:

- `GS_MENU`: the tree's own opening announcement speaks.
- `GS_VS`, `GS_SUDDEN_DEATH`, `GS_TRAINING`: matches, where the game's announcer speaks. This covers the attract demo fight.
- `GS_MEMCARD`: reading its prompts is a later feature.
- `GS_PROG_SCAN`, `GS_DEBUG_MENU`, `GS_COMING_SOON` and the unused kinds.
- A kind missing from the table. Here a gap is silent, since a new scene kind is more likely a base port addition than a fork mistake; it is logged.

Known consequences:

- "Non-Visual Melee ready" stays the first thing spoken, in `pc_a11y_init`. A scene announcement interrupts whatever is being spoken.
- A game left idle on the title screen goes round the attract loop, so the title and the movies are named again on each pass. The play test judges whether that is too much.
- The end of the opening movie looks like the title screen and takes Start straight to the main menu. The player hears "Opening movie. Press Start to skip." and then, after Start, either the title screen or the main menu.
- Online, a lost peer can end character select on its first frame, so two scene names follow each other at once. The second interrupts the first.

### Names and layout

Suggestions; the implementer may reshape them.

| File | Role |
| --- | --- |
| `menu_speech.hpp/.cpp` | The menu reader: holds the previous snapshot, compares, composes announcements. Pure: it takes a snapshot and gives text, and touches no game state. |
| `menu_names.hpp/.cpp` | The string table for the main menu tree. |
| `scene_speech.hpp/.cpp` | The scene table and its announcement. |
| `game_access.h/.c` | Filling a snapshot from game memory: the tree's state, description numbers, the Sound and Screen display values, the Online strings. |
| `menu_text.cpp` | Shrinks to the text dump, or merges into the reader. |

### Tests

- **Unit test.** A fork test feeds the menu reader snapshots and checks the announcements through the fake bridge, as `test_speech.cpp` does. It covers the rules above, including the ones about saying nothing. The scene table gets the same treatment.
- **Drive scripts.** Committed under `tools/a11y/`, walking the tree and the three leaf screens, with a `wait` on each expected `[a11y]` line, so a missing line fails the run. They are the agent's check on the real game and the guard after a base merge. The exploratory scripts of 2026-09-27 are in `.scratch/main-menu-tree/drives/` as a starting point; they wait on the old description log lines, which this feature removes.
- **Bounded run.** A `title` run still exits cleanly, and now also logs the title screen's announcement.
- **Play test.** The maintainer, by ear. Only what that confirms is called working.

### Docs

- ADR-0003: the poll, with its alternatives. Written with this spec.
- `CONTEXT.md`: thirteen terms added under "Native menus" during the session, and the boundary of "Setting".
- The primer `docs/a11y/native-menus.md` is updated by each issue where the code or these findings differ from it. Known corrections:
  - The Camera Mode scene (`GS_CAMERA_VS`) is a notice screen shown before the match, not the snapshot match.
  - Locked entries of tree screens are absent, not refused.
  - The two choke points in `mnmain.c` and the gaps of the description hook belong under the candidate hook points, with a pointer to ADR-0003.
  - The opening movie comes before the title screen on a normal launch.
- CLAUDE.md:
  - One "Accessibility status" line as each issue lands.
  - The expected `[a11y]` lines of a `title` run in "Verification" gain the scene announcement.
  - The note on `--no-card`: All-Star and Sound Test appear once Start is pressed on the title screen, not from the save itself.

## Issues

1. `issues/01-scene-announcements.md`: the scene hook and its table.
2. `issues/02-tree-screens.md`: the poll, the string table, descriptions, leaf screens named. Blocked by 01.
3. `issues/03-three-leaf-screens.md`: Sound, Screen display, Multi-Man Melee. Blocked by 02.
4. `issues/04-play-test.md`: the maintainer, by ear. Blocked by 01 to 03.

The session agreed on the tree screens first and the scene announcements as an independent issue. The order changed when writing this down: the menu reader has to forget its previous state when the scene is entered again, and the scene hook is what tells it.
