# Native menus: how the game's menus work

A primer for the work of making the native menus speak. It describes how the game builds and runs its menus, what kinds of menu it has and where each one lives in the code, and ends with candidate hook points and material for discussion. It does not decide how any menu should sound.

## Mental model

The game is a chain of scenes: the title screen, the main menu, character select, stage select, the match, the results screen. Each scene is a set of game objects, and every frame the game calls each object's update function. A menu screen is such a scene, or part of one. Its update function (the decomp calls it a "think" function) reads the controller, changes a selection number or a setting's value, plays a menu sound, and replays animations on 3D models so the highlight moves and the displayed value changes.

Nothing in that loop knows about words. What a sighted player reads comes from three places:

- **Picture labels**: entry names, character and stage names, drawn as images inside the menu's model files.
- **Values shown as pictures**: a setting's current value (On, Stereo, 3 stocks) is usually also a picture, chosen by the value's number. The fork reads the number from the game's state and supplies the words.
- **Game text**: descriptions, statistics, names and numbers drawn by the game's own text system, which the fork can in principle read back as text.

So making a menu speak means noticing when the screen, selection or value changed, reading the game's state to find out what is now selected and set, and finding words for it: from a fork-owned string table keyed by the game's identifiers, or from the game's text.

Most menu code is fully decompiled but partly unnamed: many functions and globals still carry address names such as `mn_80229894` or `mnCharSel_804D6CB0`, and many struct fields are named by offset (`x5`, `x10`). This document uses the names as they are; a later decomp sync may rename them.

## Engine terms in plain words

- **GObj** (`HSD_GObj`, `src/sysdolphin/baselib/gobj.h`): a generic game object. It owns a payload (a 3D model, a camera, a text, or a plain struct of a screen's state) and optionally a **proc**, a function the game calls once per frame. A GObj with a proc is roughly "an object with an update method". Fighters, cameras, menus and on-screen text are all built this way.
- **JObj** (`HSD_JObj`): a joint in a 3D model tree, with position, rotation, scale, children and animations. Menus load whole named model trees from the disc's archive files.
- **Texture** and **texture animation**: a texture is an image painted onto a model. A texture animation swaps which image is shown, frame by frame. Menus use it as a lookup: to show "VS Mode" or "Stereo", the code jumps the animation to the frame holding that image. That frame number is usually the same number the menu keeps as its selection or value.
- **Hit testing**: deciding which on-screen target a moving cursor is over by checking whether its position falls inside each target's rectangle.
- **Port** (in the decomp): a GameCube controller socket, numbered 0 to 3. The PC port maps keyboard and gamepads onto these virtual controllers, so the game only ever sees GameCube controller state.
- **NTSC-U and PAL**: the North American and European releases of the game. Their discs store text differently; the port supports both.
- **Special Melee**: a VS mode submenu of variant rule sets (giant fighters, slow motion, camera mode and more), each its own game mode.

## Scenes: how the game moves between screens

Scene control lives in `src/melee/gm/`. There are two levels:

- A **game mode** (`GameMode`, `src/melee/gm/types.h`), such as `GM_MENU`, `GM_VS`, `GM_CLASSIC` or the base port's `GM_ONLINE`, is a small state machine. Its states (`GameModeState`) are the steps of that mode: VS mode is character select, then stage select, then the match, then results, then back to character select. Each mode's list of states lives in the mode's own file (`gm_Mode_Vs_States` in `gmvsmode.c`, for example).
- A **scene** (`GameScene`) is the shared code a state runs, identified by a `GameSceneKind` such as `GS_MENU`, `GS_CSS` (character select), `GS_SSS` (stage select) or `GS_VS` (a match). Several modes reuse the same scene: every mode's character select is the one `GS_CSS` scene with a different mode flag.

The tables of modes and scenes are in `src/melee/gm/gmscdata.c`. The machine that runs them is in `src/melee/gm/gm_1A3F.c`: `gm_801A4014` enters one state, calls its scene's enter function, runs the scene's frames until it asks to leave, and calls the exit functions. Every scene change in the game passes through it. The current mode can be read with `gm_GetCurrentGameMode()` and the current state's id with `gm_GetCurrentSceneIndex()`. The state id is a step within the mode, not a `GameSceneKind`. No getter returns the running scene's kind, but the global `gm_804D6720` (set by `gm_801A4B88` in `gmscene.c`, just before the scene's enter function) points at the running scene's `GameSceneInfo`, whose `scene_kind` says which scene it is; the base port already reads it there.

The scene's enter function loads everything the scene needs and blocks while it does; the first frame is drawn after it returns. That return is where the fork names the scene (`pc_a11y_scene_entered`), so the announcement lines up with the first frame the player can act on. The mode passed to `gm_801A4014` is the one being run, which during a memory card interruption differs from `gm_GetCurrentGameMode()`.

A scene's kind does not always name what the player would call the screen. `GS_VS` is the match in VS mode, Classic stages, Target Test, Camera Mode and the attract demo alike; character select, stage select and results are the same scene in every mode.

A normal launch runs the memory card scene (`GS_MEMCARD`), then the opening movie (`GS_MOVIE_OPENING`, `gmopeningmode.c`), then the title screen. The memory card scene passes without showing anything when the save loads, and shows a prompt otherwise (always with `--no-card`); its kind cannot tell which. Start or A early in the movie goes to the title screen. Start late in the movie, once the logo is drawn, goes straight to the main menu and never enters the title scene. `MELEE_BOOT_SCENE` skips all of this: `title` starts on the title screen.

The whole main menu tree, from "1P Mode" down to "Sound", is a single scene (`GS_MENU`, entered through `src/melee/gm/gmmenumode.c`), so moving between its screens is not a scene change. When the player comes back from a mode, `gmmenumode.c` picks which menu screen and entry to land on (leaving Classic lands on the Classic entry), so the first announcement after returning has to read where the player landed rather than assume the top of the menu.

Some screens run inside another scene: character select opens the rules screen (with the item switch under it) and the name keyboard without leaving `GS_CSS`, and they keep their cursor in the same global state (`mn_804A04F0`, see [Candidate hook points](#candidate-hook-points)) as the main menu tree's versions. Its name tag window, where a player picks a saved name, is part of character select itself.

## Input: how menus read the controller

- Once per frame `gm_EvaluateAllControllerInputs` (`src/melee/gm/gm_1A36.c`) turns each port's raw controller state into button flags, plus a fifth slot that merges all four ports. It folds the D-pad and the control stick together (`PAD_ANY_UP` and friends), so menus cannot tell them apart, and it computes auto-repeat for held directions.
- `mn_80229624` (`src/melee/mn/mnmain.c`) turns those flags into a `MenuInput` bit set (Up, Down, Left, Right, Confirm, Back, A, Start, L, R, X, Y; enum in `src/melee/mn/inlines.h`). Confirm is A or Start; Back is B.
- Most main menu screens read the merged slot (`Menu_GetAllInputs()`), so **any controller drives them**. The exceptions:
  - character select reads each port's raw controller state for its own cursor;
  - stage select merges all ports into one cursor, unless the mode hands it one port (online, each peer follows only its own controller);
  - name entry listens to the port whose name is being typed;
  - Rumble reads each controller separately: each moves between its own port's rumble toggle and the shared list of names;
  - the training menu reads one port.
- After a screen switch the menu ignores input for a few frames (`mn_804D6BC8.cooldown`) while its animation plays.

Single-player modes remember which controller started them (`gm_801677E8`, called as the mode is chosen), and single-cursor character select is handed a port through its data (`mnCharSel_804D6CF0`).

## Sounds: the menu's own feedback

Menu sounds go through one function, `lbAudioAx_80024030(kind)` (`src/melee/lb/lbaudio_ax.c`), which plays one of eleven fixed sounds. The kinds that matter most:

- 0, back (wrapped as `sfxBack()` in `src/melee/mn/inlines.h`);
- 1, forward or confirm (`sfxForward()`);
- 2, move (`sfxMove()`);
- 3, refused: pressed on something that cannot be done (the main menu tree never plays it: its locked entries are absent);
- 5, pause.

Nearly every native menu uses these, inside and outside `src/melee/mn/`. So the game already tells a blind player *that* the cursor moved, a choice was confirmed or refused; what it cannot tell them is *what*. That makes the sound function a useful signal for the fork (see [Candidate hook points](#candidate-hook-points)).

Character select adds its own sounds: coin pick-up and drop sounds through `lbAudioAx_800237A8`, a per-character chime, and the announcer saying the character's name. The announcer is `gm_80168C5C(char_kind)` (`src/melee/gm/gm_1601.c`), which maps a character to its voice sample.

## Where the words come from

### Picture labels and values

Menu entry names, character portraits and names, stage names and previews, the HMN (human) / CPU buttons, team colours and the "Ready to Fight" banner are textures. So are most setting values: the rules values including the time digits, Stereo/Mono and the balance slider, deflicker on/off, the language, the Multi-Man Melee choice, and every training menu value. There is nothing to read back. The fork reads the number the menu keeps (a selection, a value, a character or stage id) and turns it into words: through a string table for names, directly for numbers.

The identifiers are mostly named enums: `MenuKind` and the per-screen selection enums in `src/melee/mn/forward.h`, `CharacterKind` for characters and `StKind` (`src/melee/gr/forward.h`) for stages.

### Game text

The game's text system (called SIS in the decomp; `HSD_Text` in `src/sysdolphin/baselib/sislib.h`) draws the main menu's description line, the one-line description under a setting row, results statistics, trophy names and descriptions, records, event match details, name tags and the names in lists, numbers on character select, memory card prompts and more. Text reaches the screen in two ways:

- **By number.** Each screen loads its own string archive from the disc: `SdMenu.usd` for the main menu tree, `SdSlChr.usd` for character select, `SdTrain.usd` for training, `SdRst.usd` for results, `SdToy.usd` and `SdToyExp.usd` for trophy names and descriptions, `SdMsgBox.usd` for message windows. Code picks a string with `HSD_SisLib_803A6368(text, index)`.
- **Formatted at runtime.** `HSD_SisLib_803A6B98(text, x, y, fmt, ...)` takes a printf-style C string and encodes it; `HSD_SisLib_803A70A0` rewrites one line of an existing text the same way. The original game uses these for numbers and names (statistics, times, the name list, event levels); the base port uses them for its own screens, whose strings the disc does not have (the Online submenu, the online lobby, the ranked stage list). At these two functions the text is still a C string, but not always ASCII: the original game's strings often mix in Shift-JIS (the Japanese character encoding the game was written with), such as full-width letters in character names, so a reader needs a Shift-JIS decoder. Character names come from a C table, `gm_80160980` (`src/melee/gm/gm_1601.c`), a ready source of names.

Strings by number are stored as a byte code rather than plain characters (`src/sysdolphin/baselib/sislib.static.h` and `hsd_3A64.c`): commands (colour, spacing, line breaks, and jumps into another string) and glyph codes, a glyph being one drawn character. Most glyphs come from a font atlas (one image holding the characters shared by all text) whose table from Shift-JIS to glyph codes, used by `HSD_SisLib_803A67EC` to encode C strings, can be run backwards to decode a string at runtime. The fork's decoder, `decode_game_text` (`src/pc/a11y/game_text.cpp`), does that; [Decoding game text](#decoding-game-text) has what a first run over the main menu's strings found. NTSC-U and PAL discs encode glyphs and order strings differently; `sis_opcode`/`sis_glyph` and `pc_region_sis_index()` (`src/pc/region.c`) handle that, and a decoder has to go through them.

The `.usd` archives are the English strings and the `.dat` ones the Japanese, chosen by the in-game language setting; a few archives exist only as `.dat` and serve both (`SdIntro.dat`).

Most game text passes through those three functions, but not all. `HSD_SisLib_803A6530` and `HSD_SisLib_803A660C` copy or append one string table entry into another at runtime (camera mode and the unlock notice do), so a string's number does not always identify its content; character select builds some text buffers by hand; and on PAL discs `HSD_SisLib_803A6368Raw` binds strings without going through `HSD_SisLib_803A6368`.

### Decoding game text

The first experiment decoded every string of `SdMenu.usd` on an NTSC-U disc, all 1604 of them, and the description line as the player moves through the main menu tree. Run it with `MELEE_A11Y_TEXT_DUMP=1` and a drive to the main menu; the whole table is logged on the menu scene's first frame.

- **The English text decodes.** Every string came out as the screen shows it, with no command the decoder did not know; the decoder follows the jump and call commands and skips colour, scale, spacing, alignment and timing by their operand sizes, as the renderer (`HSD_SisLib_803A84BC`) does.
- **The atlas table is exact.** The encoder's two tables (`lbl_8040C8C0`, Shift-JIS per entry, and `HSD_SisLib_8040C680`, glyph code per entry) pair up one to one over all 287 atlas glyphs: digits, Latin letters, kana, punctuation and 23 kanji. The decoder keeps its own copy, since PAL rewrites `HSD_SisLib_8040C680` at startup.
- **Font glyphs are few.** A glyph code of 0x4000 or above comes from the loaded archive's own font instead of the atlas and has no character code. In `SdMenu` they are, read from context, 0x4000 for é (Pokémon) and 0x4002 for the numeral II (Mushroom Kingdom II); besides those, 0x4001 filling twelve three-glyph strings of unknown use, and 0x4003 to 0x4006 inside two Japanese stage names left in the English table. A per-font table of a few entries covers a screen's text.
- **Not every entry is text.** Some are placeholders for text built at runtime (four long runs of お, sized buffers), locked-entry stand-ins ("???", "? ? ?"), units and number pieces that code combines ("%", "ft.", "x-1000"). Strings keyed by number need the code that uses them to know which is which.
- **Line breaks are layout.** A description wraps where the screen needs it ("Conquer all enemies using / limited recovery items."), so speech joins the lines with a space.
- **A table has no length.** It is an array of pointers with nothing marking its end; the first slot that resolves outside game memory ends the dump, which is a heuristic that gave a plausible last string here.
- **Unverified on PAL.** PAL glyphs are one byte, the printable ASCII range decodes by rule, and the accented letters after it are unmapped.

The description line itself is rebuilt by `mn_80229A7C` (`mnmain.c`) each time the hovered entry changes on a tree screen that has descriptions (main menu, 1P, VS, Trophies, Options, Data, Regular Match, Stadium, Special Melee, Records) and when such a screen appears. As a "hovered entry changed" signal it has gaps: it is never reached for Online or the Online entry of VS. Mode, whose text the base port draws in a branch that returns first, and a move made while a screen slides in is not rebuilt, so the screen can briefly show the previous entry's description. The fork therefore looks descriptions up itself, from `mn_803EB6B0[menu].description_indices[selection]`, through `pc_region_sis_index()`.

## Kinds of native menu

The screens fall into a handful of interaction patterns. The names are working names; see [Candidate vocabulary](#candidate-vocabulary). [Screen inventory](#screen-inventory) places most screens in one of these kinds; a few large ones (snapshots, tournament, trophies) mix several.

- **Tree menus**: a vertical list of entries. Up and Down move, Confirm enters a submenu or starts a mode, Back goes up a level. Lists wrap at both ends. The main menu tree is made of these; its locked entries (All-Star and Sound Test until unlocked) and its three removed ones are absent, not refused: not drawn, skipped by the cursor, and no refused sound.
- **Setting rows**: a list of rows, each with a value. Up and Down pick a row, Left and Right change the value. Some rows open a sub-screen instead.
- **Toggle grids**: a grid of on/off items with a moving highlight, where A toggles the highlighted item.
- **Free-cursor screens**: a cursor moved with the analog stick over a 2D layout, with the selection decided by hit testing. These are the hardest to make accessible: positions are continuous, there is empty space between targets, and on character select up to four cursors move at once.
- **Scrolling lists and viewers**: lists of records or content to browse, mostly without values to change.
- **On-screen keyboard**: a 2D grid of characters with a cursor, and a caret in the text being typed.
- **Prompts**: a message with a Yes/No choice or "press a button to continue".
- **Per-player pages**: each player pages through their own view independently.

## Screen inventory

Grouped by where the player meets them. For each screen, the kind and where its words come from.

### Boot and title

- **Memory card prompts** (`src/melee/gm/gmscmemcard.c`, message window in `gm_1ADD.c`): prompts, each a Yes/No toggle; game text (`SdMsgBox`).
- **Progressive scan prompt** (`src/melee/gm/gmprogressive.c`): a prompt, never shown on PC because the port reports no progressive-scan cable.
- **Opening movie** (`src/melee/gm/gmopeningmode.c`, `GS_MOVIE_OPENING`): before the title screen on every normal launch; see [Scenes](#scenes-how-the-game-moves-between-screens) for how Start leaves it.
- **Title screen** (`src/melee/gm/gmtitle.c`, `gmtitlemode.c`): "Press Start", no menu. Left alone for 600 frames it starts the attract loop: a demo fight, the title again, another demo fight, the how-to-play movie, the special movie.

### The main menu tree

All in `src/melee/mn/`, one scene. `MenuKind` and the selection enums (`SEL_MAIN_*`, `SEL_1P_*`, `SEL_VS_*`, ...) in `forward.h` describe the tree; a few `MenuKind` values are still unnamed placeholders (`MENU_KIND_8`, `MENU_KIND_27`, ...).

- **Main menu, 1P, VS, Trophies, Options, Data, and their submenus** (`mnmain.c`): tree menus. Entry names are pictures; the description line at the bottom is game text by number.
- **Online, under VS** (`mnonline.c`, base port): a tree menu whose labels and descriptions are plain C strings, available through `mnOnline_Label` and `mnOnline_Description`.
- **Rules and additional rules** (`mnmainrule.c`, `mnruleplus.c`): setting rows. Names and values are pictures; the description of the current row or value is game text.
- **Item switch** (`mnitemsw.c`): toggle grid of 31 items plus an overall frequency row; pictures.
- **Random stage switch** (`mnstagesw.c`): toggle grid of 29 stages; stage names are game text.
- **Name entry**: the name list (`mnname.c`, a scrolling list, names formatted at runtime) and the keyboard (`mnnamenew.c`).
- **Options**: Rumble (`mnvibration.c`, per-port toggles plus a list of names with their own setting, names as game text), Sound (`mnsound.c`), Screen display (`mndeflicker.c`), Language (`mnlanguage.c`, which reboots into the menu when changed), Erase data (`mndatadel.c`, a list plus a Yes/No confirmation). Setting rows and prompts. Values are pictures; the text in the middle of the screen describes the current row. Screen display's description is written "ON : Display will be ..." in the game's text, with a space before each colon.
- **Event Match, under 1P** (`mnevent.c`): a scrolling list of events. The event's name and description are game text by number; its level and best record are formatted at runtime.
- **Multi-Man Melee, under 1P and Stadium** (`mnhyaku.c`): one row of six choices; the choice is a picture, its description game text.
- **Data**: records (`mninfo.c`, `mninfobonus.c`, `mncount.c`, `mndiagram.c`, `mndiagram2.c`, `mndiagram3.c`; scrolling lists, and a 2D table of fighters or names against statistics whose headers are character pictures in the fighter view and names in game text in the name view), sound test (`mnsoundtest.c`, two rows: a category and a track), snapshot album (`mnsnap.c`, large and with several modes), movie gallery (`mngallery.c`, a scrolling list).

### Online

- **Online lobby** (`src/melee/gm/gmonlinemode.c`, drawn by `src/melee/mn/mnonlinelobby.c`, base port): the first scene of its own mode, `GM_ONLINE`, so entering it is a scene change. Every frame `gmonlinemode.c` fills an `OnlineLobbyView` (`src/melee/gm/gmonlinemode.h`) with the lobby phase, the players found (name, ping, whether host), the rows, the highlighted row, a message, a hint and the keyboard for a friend's connect code, and a title and subtitle; its text is plain strings. The view is a clean place to read the whole screen from.

### Before a match

- **Character select** (`src/melee/mn/mncharsel.c`): free cursors. See [Character select in detail](#character-select-in-detail).
- **Stage select** (`src/melee/mn/mnstagesel.c`): one free cursor over a grid of 29 stages and a random cell. The hovered cell is the global `mnStageSel_804D6CAE`, an index into the table `mnStageSel_803F06D0`, whose rows hold each cell's `StKind`. It starts at `0x1E`, meaning no cell (Start then picks a random stage), changes when the cursor enters any visible cell, and keeps the last cell while the cursor crosses empty space. A locked stage in the main grid is still a visible cell, drawn as a "?" (its row's `x8` is 1; unlocked is 2), and A on it plays the refused sound, so a reader must not speak a locked stage's real name. Locked stages outside the main grid are hidden (`x8` is 0) and never hovered. Stage names and previews are pictures. The layout (see `docs/screenshots/stage-select.png`): two rows of stage pictures at the top (the first with one extra cell set apart at the far left); below them, the random cell on the left beside a row of five stages, and a row of "?" cells under those five. Ranked online play replaces the grid with a text list of plain strings (`rankStage*` functions).

Every mode's character and stage select is the same screen: the mode is a `CSSMatchType` value in the screen's data, and single-player types switch the screen from four cursors to one.

### During a match

- **Pause** (`src/melee/gm/gmvs.c`, overlay in `gmpause.c`): no menu. Start pauses and gives the pausing player a free camera; L+R+A+Start quits the match as a no-contest.
- **Training menu** (`src/melee/gm/gm_1884.c`): shown while training is paused. Setting rows (speed, item, number of CPUs, CPU behaviour, damage, camera, reset, exit, and one hidden row the cursor skips; A on the item row spawns the item); the row is `gm_80473814.x00`, the values `gm_80473814.menu_values`. Values are pictures; only the chosen item's name and a pause notice are game text.
- **Camera mode** (`src/melee/gm/gmcameramode.c`): the Special Melee match for taking snapshots. Before character select, a notice screen (`GS_CAMERA_VS`, `gmcamera.c`) explains the controls in game text (`SdVsCam`); the match itself is an ordinary `GS_VS` scene.

### After a match and between matches

- **Results** (`src/melee/gm/gmresult.c`, `gmresultplayer.c`): per-player pages of summary and statistics, game text (`SdRst`). The screen moves on when every human player has pressed Start.
- **1P mode screens**: intro splashes (`SdIntro`), the Continue decision (`gmregclear.c`, `SdDec`), clear screens (`SdClr`), game over and "coming soon" scenes, "New Challenger" (`gmapproach.c`). Mostly sequences with one prompt.
- **Unlock notice** (`src/melee/if/ifprize.c`, `GS_PRIZE_INTERFACE`): shown after a VS or Special Melee match, or a challenger fight, that unlocked something.
- **Tournament** (`src/melee/gm/gmtou_0.c` for setup, `gmtou_1.c` for the bracket, `gmtou_2.c` for choices between rounds): cursor menus with game text. Large, only skimmed.
- **Staff roll** (`src/melee/gm/gmstaffroll.c`): a free cursor that shoots floating names during the credits; ends on its own.

### Trophies

The trophy screens' interaction lives in `src/melee/ty/` (`toy.c`, `tylist.c`, `tydisplay.c`, `tyfigupon.c`); the `gmtoy*.c` files in `gm/` are only mode shells. The gallery and collection show a trophy that the player rotates and zooms, with its name (`SdToy`) and description (`SdToyExp`) in game text; the lottery is a machine the player feeds coins. Only skimmed.

### Developer screens

`GS_DEBUG_MENU` (`src/melee/gm/gmdebugmode.c`) and `gmhanyucss.c` / `gmhanyusss.c`, which cycle character and stage select through every mode's variant, exist only for developers and are out of scope.

## Character select in detail

The most complex native menu, and the one no player can avoid. The layout (see `docs/screenshots/character-select.png`): the rules header ("2-minute KO fest!") across the top, the Back button in the top-right corner, three rows of character portraits in the middle, with locked characters shown as "?", and the four player slots along the bottom, each with its HMN/CPU tab, its name box and, for a CPU, its level slider.

- **Doors** are the four player slots along the bottom (`CSSDoor`, `src/melee/mn/types.h`, array `mnCharSel_803F0DFC.doors`). Each holds the slot's player kind (`p_kind`: human, CPU or closed), `team`, `costume`, and `sel_icon`, the portrait this slot's coin is on or was last over (`0x19` once the coin is picked up, until it reaches a portrait). While a carried coin is over empty space, `sel_icon_prev` is `0x19`. "Door" is the decomp's word; players say "player slot". The character actually chosen for the match is written to `mnCharSel_804D6CB0->vs.start.players[i].ckind`.
- **Cursors** are the hands, one per active port (`CSSCursorData`, local to `mncharsel.c`, every field still unnamed): `x4` is the port, `x5` the hand's state (free, carrying a coin, or holding a slider), `x6` what it holds (a door's coin, or a door's CPU-level or handicap slider), `xC` and `x10` its position in menu units.
- **Icons** are the portraits (`static CSSIcon icons[]` in `mncharsel.c`): a rectangle for hit testing, the character (`char_kind`, a `CharacterKind`), a locked/unlocked state and the chime sound. Unlike a locked stage, a locked portrait ("?") is never hovered: the hit test skips it. `SelectableCharacterKind` in `types.h` names the portrait order.
- **The coin**: picking a character means carrying a coin onto a portrait and pressing A. While the coin is over a portrait, the screen sets that door's `sel_icon` every frame; moving off a portrait leaves `sel_icon` unchanged. A commits the portrait, plays the chime and calls the announcer. A hand can carry another door's coin, which is how a human picks a character for a CPU.
- **Other targets**: the HMN/CPU toggle and team button on each door, the CPU-level and handicap sliders, the name tag window (a list of saved names, game text), the rules header at the top centre (opens the rules screen inside character select), and the Back button in the top-right corner (holding B does the same). X and Y change the costume (`mnCharSel_CostumeChange`, one of the few named functions).
- **Ready to Fight** is a state, not a target: the banner appears (`mnCharSel_804D6CF7` nonzero) when every open slot has a character, enough players are in (at least two teams in a team match), no coin is held and no name tag window is open; in single-player modes, once the one coin is placed. Start from any active controller then confirms the choices and leaves the screen; Start before that plays the refused sound.
- **Multiple players**: in VS modes there are four cursors, one per controller, each independent; in single-player modes one. Online, the remote player's cursor moves on this screen too, driven by their synced controller.

Everything a sighted player reads here is a picture except numbers (rules header, CPU level, handicap, KO counts), timers, name tags and the character's name in the name tag window.

## Candidate hook points

Speculative: places the code offers, not decisions.

- **Scene changed**: `gm_801A4014` in `gm_1A3F.c`, right after the scene's enter function. One hook sees every scene change in the game, with the scene's kind, and can announce the scene or hand over to a screen-specific reader.
- **Main menu tree, tree screens**: the tree screens in `mnmain.c` keep the current screen and highlighted entry in the global `mn_804A04F0` (`MenuFlow`, `src/melee/mn/mnmain.h`: `cur_menu`, `prev_menu`, `hovered_selection`). There is no single function for "selection moved" or "screen changed": each tree screen has its own think function that writes `cur_menu` inline both ways, and `mn_80229894` is what leaf screens call to return to the tree (and what the base port uses to open Online). Polling `cur_menu` and `hovered_selection` once a frame catches all of it; this is the case the isolation rule allows polling for, and the fork polls once a frame from the menu scene's own frame function, which runs before the menu's objects update. The poll is scoped to `GS_MENU`; the rules screen, item switch and name keyboard inside `GS_CSS` would need a second call site. The alternatives, and why the poll won, are in `docs/adr/0003-main-menu-tree-is-polled.md`:
  - two choke points in `mnmain.c`: `mn_8022B3A0` builds every tree screen, and the selection-changed branch of `fn_8022AFEC` sees every hover change. They cover the tree screens exactly, but no leaf screen passes through either, so every leaf screen would need its own hook;
  - the description line (`mn_80229A7C`), which misses Online and moves made during a slide (see [Decoding game text](#decoding-game-text)).
- **Main menu tree, leaf screens**: the screens at the ends of the tree need per-screen readers, because where they keep their state varies:
  - rules, additional rules, the item and stage switches, the name list, the name keyboard and the records table use `hovered_selection` too (the records table packs row and column into it), with a value in `confirmed_selection` or their own struct;
  - Sound, Screen display, Language and Multi-Man keep their cursor in the generic `Menu` struct, private to their file. All four set the text in the middle of the screen through one shared helper, `Menu_InitCenterText` (`src/melee/mn/inlines.h`), each time the row or choice changes and once as the screen opens. The number of that text names the row or choice, so a hook in the helper (`pc_a11y_menu_center_text`) stands in for reading the private cursor; Screen display and Language have one row and set theirs once. The opening function sets `cur_menu` and the centre text in the same frame, so the poll sees both together; the fork keeps the last number across scenes, since after a Multi-Man match the menu scene opens straight on that screen before the scene hook runs. The values need no hook: each press saves them at once, Sound's channel as the synth's sound mode (`HSD_SynthGetSoundMode`, 1 for stereo; `lbAudioAx_80024BD0` reads the same but also writes a copy) and its balance and Screen display's deflicker in the game preferences (`gmMainLib_8015ED74`, -100 to +100 in steps of 5; `gmMainLib_8015F4E8`). The balance runs the opposite way to how its bar looks: Right moves the divider toward the MUSIC label, but it grows the SOUNDS part of the bar, and +100 silences the music. Screen display ignores A until its slide-in animation ends;
  - every other screen keeps its own state (Rumble in `MnVibrationData`; event match, the other records pages, the sound test, the snapshot album, the movie gallery, Erase data).
- **Something happened**: `lbAudioAx_80024030`, the menu sound function. One hook there tells the fork the player moved, confirmed, cancelled or was refused, on almost every native menu, including screens outside `mn/`. It carries no context, but it tells a poller when to look, and the refused sound explains a press that changed nothing.
- **Training menu**: `gm_80473814.x00` (row) and `menu_values`, read when a move sound plays.
- **Character select**: changes of `doors[i].sel_icon_prev` (a carried coin entered or left a portrait; `sel_icon` alone misses leaving and re-entering the same one), of `p_kind` (human, CPU or closed), `mnCharSel_CostumeChange`, the confirm path in `mncharsel.c` that calls the announcer (a character was picked; hook the character select call sites, not `gm_80168C5C` itself, which other screens call too), and `mnCharSel_804D6CF7` going from zero to nonzero (Ready to Fight shown; while shown it counts animation frames).
- **Stage select**: changes of `mnStageSel_804D6CAE`.
- **Game text**: the three functions in [Game text](#game-text) see most text as it is set. Whether a generic text reader built on them is useful, or too noisy without knowing which screen a text belongs to, is open.
- **Online lobby**: the `OnlineLobbyView` that `gmonlinemode.c` fills each frame.

## Design constraints

- **Many players, one menu.** Shared menus accept any controller, character select has one cursor per controller, and online a second human moves on the same screens. Whose actions to announce is a decision to make.
- **Free cursors need more than "read the item".** On character and stage select the cursor slides between targets over empty space; announcing the target the cursor enters is the obvious start, but a player also needs to find targets.
- **Read, don't steer.** Online, character and stage select run in lockstep: both machines advance frame by frame on the same synced controller input, and rollback (re-running recent frames when a late input arrives) is switched on only once the match starts (`docs/netcode-plan.md`, native menu integration). A fork feature that changed menu state or fed input would desynchronise the peers. Reading state and speaking is safe; the netplay speech gate (`pc_net_resim()`) costs nothing in menus.
- **Names can move.** Hooks go into decomp-layer files, which decomp syncs replace. Hooking named functions and reading named fields is sturdier than reading `x5`-style fields, and every hook of this kind is a merge risk worth keeping small.
- **Region.** String numbers are the NTSC-U build's, remapped for PAL discs by `pc_region_sis_index()`. String tables keyed by game identifiers (menu kind and selection, character, stage) are unaffected.

## For discussion

This section is a snapshot for the discussions ahead, not a description of the code; later work supersedes it.

### Where screenshots would help

Code gives the logic but not the layout. `docs/screenshots/` already covers the title, main menu, VS mode submenu, character select and stage select, and `docs/screenshots/macos-pal/` adds results and trophies (a PAL disc). Still missing, as screenshots or a sighted helper's description:

- how far apart character select and stage select targets are in cursor movement, and how the cursor feels to steer;
- the item switch and random stage switch grids;
- the name entry keyboard layout;
- the training menu rows and their values;
- the snapshot album and the trophy gallery in detail.

### Candidate vocabulary

Terms this document uses that may deserve a place in `CONTEXT.md`:

- **Menu screen**: one screen of the main menu tree, a `MenuKind` (the tree is one scene holding many screens).
- **Hovered entry** versus **chosen entry**: what the cursor is on versus what the player confirmed.
- **Tree menu**, **setting row**, **toggle grid**, **free-cursor screen**, **prompt**: the kinds above.
- **Picture label**, **value** and **game text**: the three sources of what a sighted player reads.
- **Player slot** (the decomp's door) and **coin** on character select.

### Open questions

- Does decoding game text by number work for all English strings? For the main menu's table it does (see [Decoding game text](#decoding-game-text)); still to check are the other archives (character select, training, results, trophies, message windows), their font glyphs, and a PAL disc.
- Where a string table entry is patched at runtime, is its number still a usable key?
- How should the fork decide which controller is the blind player's on character select?
- How much of the trophy, snapshot and tournament code is worth reading before those screens are in scope?
