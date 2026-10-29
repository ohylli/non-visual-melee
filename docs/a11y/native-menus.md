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

Some screens run inside another scene: character select opens the rules screens (Custom Rules and the three screens it leads to) and the name keyboard without leaving `GS_CSS`, and they keep their cursor in the same global state (`mn_804A04F0`, see [Candidate hook points](#candidate-hook-points)) as the main menu tree's versions. Its name tag window, where a player picks a saved name, is part of character select itself.

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

### How a stick value reaches a cursor

What the fork needs to know to move a free cursor by feeding stick input, as steering on character select does (`docs/adr/0004-free-cursors-are-steered-through-the-controller.md`).

- **One thread.** Once per video frame, `pc_frame_boundary` (`src/pc/vi.c`) pumps events and publishes port 1's virtual pad (`publish_locked` in `src/pc/keyboard.c`, which merges the keyboard, the key driver and a GameCube adapter). The pad alarm then samples the pads (`PADRead`, which merges the virtual pad with an SDL gamepad, adding their stick axes and clamping the sum to a full push), and the simulated frames run. All of it is on the game thread, so a value the game's code asks for reaches the pad without crossing threads.
- **The game's clamping** (`HSD_PadClamp`, set up in `gmmain.c` with a minimum of 0 and a maximum of 80) sits between the sample and `HSD_PadCopyStatus`, which the screens read. Measured on 2026-09-30 by publishing every value and logging what the hand's port read: every stick within a circle of 80 arrives exactly as sent, down to 1; anything further out is scaled back onto the circle, each axis truncated toward zero (127,0 arrives as 80,0; 100,50 as 71,35; 64,64 as 56,56).
- **The cursor's movement** on character select is `0.0002 * (x² + y² - 200)` units a frame in the stick's direction, and nothing below a squared tilt of 200. A full push moves 1.24 units, and the smallest tilt that moves anything moves 0.005, so a cursor can be placed to a hundredth of a unit.
- **Delay.** Offline, a stick published at one video frame's boundary moves the cursor in the next simulated frame. Online, the local sample is taken once per fresh simulated frame and applied the input delay later, 2 frames by default. Video frames and simulated frames are not one to one: two simulated frames in one video frame sample the same published pad twice, and a video frame that simulates nothing loses what was published in it.
- **The D-pad** reaches `HSD_PadCopyStatus` as its own buttons, apart from the stick's direction flags, and character select ignores it; a press is visible in the port's `trigger` bits for the frame it began.

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

- **By number.** Each screen loads its own string archive from the disc: `SdMenu.usd` for the main menu tree, `SdSlChr.usd` for character select, `SdTrain.usd` for training, `SdRst.usd` for results, `SdPrize.usd` for the unlock notices, `SdToy.usd` and `SdToyExp.usd` for trophy names and descriptions, `SdMsgBox.usd` for message windows. Code picks a string with `HSD_SisLib_803A6368(text, index)`.
- **Formatted at runtime.** `HSD_SisLib_803A6B98(text, x, y, fmt, ...)` takes a printf-style C string and encodes it; `HSD_SisLib_803A70A0` rewrites one line of an existing text the same way. The original game uses these for numbers and names (statistics, times, the name list, event levels); the base port uses them for its own screens, whose strings the disc does not have (the Online submenu, the online lobby, the ranked stage list). At these two functions the text is still a C string, but not always ASCII: the original game's strings often mix in Shift-JIS (the Japanese character encoding the game was written with), such as full-width letters in character names, so a reader needs a Shift-JIS decoder. Character names come from a C table, `gm_80160980` (`src/melee/gm/gm_1601.c`), a ready source of names.

Strings by number are stored as a byte code rather than plain characters (`src/sysdolphin/baselib/sislib.static.h` and `hsd_3A64.c`): commands (colour, spacing, line breaks, and jumps into another string) and glyph codes, a glyph being one drawn character. Most glyphs come from a font atlas (one image holding the characters shared by all text) whose table from Shift-JIS to glyph codes, used by `HSD_SisLib_803A67EC` to encode C strings, can be run backwards to decode a string at runtime. The fork's decoder, `decode_game_text` (`src/pc/a11y/game_text.cpp`), does that; [Decoding game text](#decoding-game-text) has what a first run over the main menu's strings found. NTSC-U and PAL discs encode glyphs and order strings differently; `sis_opcode`/`sis_glyph` and `pc_region_sis_index()` (`src/pc/region.c`) handle that, and a decoder has to go through them.

The `.usd` archives are the English strings and the `.dat` ones the Japanese, chosen by the in-game language setting; a few archives exist only as `.dat` and serve both (`SdIntro.dat`).

Most game text passes through those three functions, but not all. `HSD_SisLib_803A6530` and `HSD_SisLib_803A660C` copy or append one string table entry into another at runtime (camera mode and the unlock notice do), so a string's number does not always identify its content; character select builds some text buffers by hand; and on PAL discs `HSD_SisLib_803A6368Raw` binds strings without going through `HSD_SisLib_803A6368`.

### Decoding game text

The first experiment decoded every string of `SdMenu.usd` on an NTSC-U disc, all 1604 of them, and the description line as the player moves through the main menu tree. Run it with `MELEE_A11Y_TEXT_DUMP=1` and a drive to the main menu; the whole table is logged on the menu scene's first frame, character select's (`SdSlChr`) on its first frame, results' (`SdRst`) on its first tick, the unlock notices' (`SdPrize`, font slot 2) at the first notice, and the memory card scene's message window (`SdMsgBox`, font slot 3) on its first frame (a `drive.py --card` run, or `--scene ""`).

- **The English text decodes.** Every string came out as the screen shows it, with no command the decoder did not know; the decoder follows the jump and call commands and skips colour, scale, spacing, alignment and timing by their operand sizes, as the renderer (`HSD_SisLib_803A84BC`) does.
- **The atlas table is exact.** The encoder's two tables (`lbl_8040C8C0`, Shift-JIS per entry, and `HSD_SisLib_8040C680`, glyph code per entry) pair up one to one over all 287 atlas glyphs: digits, Latin letters, kana, punctuation and 23 kanji. The decoder keeps its own copy, since PAL rewrites `HSD_SisLib_8040C680` at startup.
- **Font glyphs are few.** A glyph code of 0x4000 or above comes from the loaded archive's own font instead of the atlas and has no character code. In `SdMenu` they are, read from context, 0x4000 for é (Pokémon) and 0x4002 for the numeral II (Mushroom Kingdom II); besides those, 0x4001 filling twelve three-glyph strings of unknown use, and 0x4003 to 0x4006 inside two Japanese stage names left in the English table. A per-font table of a few entries covers a screen's text: the decoder takes one with its source (`GameTextSource::font_glyphs`), given so far for the unlock notices' table alone ([Notice text](#notice-text)); elsewhere these glyphs are left out of speech and logged.
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

- **Memory card prompts** (`src/melee/gm/gmscmemcard.c`, message window in `gm_1ADD.c`): prompts, a message or a choice of two (Yes and No, OK and Try Again); game text (`SdMsgBox`). See [Memory card prompts in detail](#memory-card-prompts-in-detail).
- **Progressive scan prompt** (`src/melee/gm/gmprogressive.c`): a prompt, never shown on PC because the port reports no progressive-scan cable.
- **Opening movie** (`src/melee/gm/gmopeningmode.c`, `GS_MOVIE_OPENING`): before the title screen on every normal launch; see [Scenes](#scenes-how-the-game-moves-between-screens) for how Start leaves it.
- **Title screen** (`src/melee/gm/gmtitle.c`, `gmtitlemode.c`): "Press Start", no menu. Left alone for 600 frames it starts the attract loop: a demo fight, the title again, another demo fight, the how-to-play movie, the special movie.

### The main menu tree

All in `src/melee/mn/`, one scene. `MenuKind` and the selection enums (`SEL_MAIN_*`, `SEL_1P_*`, `SEL_VS_*`, ...) in `forward.h` describe the tree; a few `MenuKind` values are still unnamed placeholders (`MENU_KIND_8`, `MENU_KIND_27`, ...).

- **Main menu, 1P, VS, Trophies, Options, Data, and their submenus** (`mnmain.c`): tree menus. Entry names are pictures; the description line at the bottom is game text by number.
- **Online, under VS** (`mnonline.c`, base port): a tree menu whose labels and descriptions are plain C strings, available through `mnOnline_Label` and `mnOnline_Description`.
- **Custom Rules and Additional Rules** (`mnmainrule.c`, `mnruleplus.c`): setting rows. Names and values are pictures; the description of the hovered row is game text, chosen by number from each screen's table (`mn_803EC818`, `mn_803ED2E8`): one per value on Rules, Handicap, Stage Selection, Friendly Fire, Pause, Score Display and Self-Destructs, one per row elsewhere, and the stock count its own. Additional Rules hides Score Display and Random Stage until their unlocks (see [Score Display](#score-display)).
- **Item Switch and Random Stage** (`mnitemsw.c`, `mnstagesw.c`): toggle grids, 31 items in two columns under a frequency strip, and 29 stages in two columns; no description line. The names are game text, the On and Off marks pictures (see [The switch screens](#the-switch-screens)).
- **Name entry**: the name list (`mnname.c`, a scrolling list, names formatted at runtime) and the keyboard (`mnnamenew.c`).
- **Options**: Rumble (`mnvibration.c`, per-port toggles plus a list of names with their own setting, names as game text), Sound (`mnsound.c`), Screen display (`mndeflicker.c`), Language (`mnlanguage.c`, which reboots into the menu when changed), Erase data (`mndatadel.c`, a list plus a Yes/No confirmation). Setting rows and prompts. Values are pictures; the text in the middle of the screen describes the current row. Screen display's description is written "ON : Display will be ..." in the game's text, with a space before each colon.
- **Event Match, under 1P** (`mnevent.c`): a scrolling list of events. The event's name and description are game text by number; its level and best record are formatted at runtime.
- **Multi-Man Melee, under 1P and Stadium** (`mnhyaku.c`): one row of six choices; the choice is a picture, its description game text.
- **Data**: records (`mninfo.c`, `mninfobonus.c`, `mncount.c`, `mndiagram.c`, `mndiagram2.c`, `mndiagram3.c`; scrolling lists, and a 2D table of fighters or names against statistics whose headers are character pictures in the fighter view and names in game text in the name view), sound test (`mnsoundtest.c`, two rows: a category and a track), snapshot album (`mnsnap.c`, large and with several modes), movie gallery (`mngallery.c`, a scrolling list).

#### Score Display

Additional Rules shows its Score Display row only while save bit `x186C & 1` is set (`gmMainLib_8015EE0C`), and its Random Stage row while `x186C & 2` is (`gmMainLib_8015EE44`). These bits are not where the unlocks are kept: every unlock check (`gm_80172898`, run on each Start on the title screen, after each VS match and at the end of most single-player modes) rewrites them from the recorded unlocks (`fn_8017280C`). Score Display's unlock, number 0x17, is recorded only by the check after a VS match, once the save's count of KOs in VS matches (`unk_1A8.x0`, raised by `gm_80162A98`) has reached 5000 (`fn_80173644`). Random Stage's, 0x18, is recorded by any check once the main stages are all unlocked (`fn_80173510`), which they are in a `--no-card` save. So agent runs show Random Stage and never Score Display, and a value forced into `x186C` before the title screen is undone by its Start. With the test forced true, the row draws between Pause and Self-Destructs and works like its neighbours.

#### The switch screens

Item Switch draws each column of item names as one multi-line game text, SdMenu strings 2 and 3; one language setup draws string 5 instead of 3, which says "Proximity Mine" for the Motion-Sensor Bomb. Random Stage draws each stage's name as its own game text, chosen through the screen's row to stage switch table (`mnStageSw_803ED4C4`, then `mnStageSw_stageIcons`), or "?????" for a locked stage, which the cursor skips. The fork still names both from its own tables, so the words do not depend on the table a scene loaded: the "é" of "Poké" is a glyph the fork's decoder does not know, and the stage screen draws "Temple" where stage select's table, which the fork reuses, says "Hyrule Temple".

- **Values.** `confirmed_selection` holds the hovered row's value on both screens: the input function sets it from the screen's own data on every move, flips it on A and steps the frequency on the strip's Left and Right, and the screen's object copies it back into its data the same frame. `GamePrefs.item_mask` and `stage_mask` are written from that data only at the next A press or on leaving, so they lag for the row changed last. Random Stage's data is static to its file, which is one more reason to read `confirmed_selection`.
- **Item Switch's frequency strip** is two hovered values, 31 entered from the left column and 32 from the right; its value runs 0 None to 5 Very High (`GamePrefs.item_freq` is one less), with Very High at the left end, so Left is more.
- **A refused toggle** leaves no trace: turning off the last stage that is on plays the refused sound and changes nothing. The fork learns of it from a hook at that branch, `pc_a11y_stage_switch_refused`.

#### The rules screens on character select

The rules header opens the same four screens inside character select's scene (see the bullet on the rules screens and name entry in [Character select in detail](#character-select-in-detail)), run by the same functions, which keep their state in `mn_804A04F0` as in the main menu tree. The main menu tree's frame hook does not run there, but character select's does, so the fork feeds the same reader from it.

- **The pending exit is the gate.** The frame that asks for the screens (pending exit 3) still shows the menu tree's last screen in `cur_menu`, which the opening function sets on the next frame along with the pending exit's 5; name entry uses 5 too, after 4. After the screens close, `cur_menu` keeps its last value. So the screens are open while the pending exit is 5 and was last 3.
- **`SdSlChr` holds the rules screens' strings.** Character select loads its own table into font slot 0, and the screens draw the same numbers from it. On an NTSC-U disc its strings 0 to 73 are `SdMenu`'s 0 to 73, character for character: the item columns (2 to 5), the stage names (6 to 37) and the rule descriptions (38 to 65), followed by name entry's text. From 74 on it has its own: 74 is the scratch line the rules header is composed into, then the header's pieces ("KO fest!", "survival test!", "coin fest!", "skills test!", "Take Snapshots!", "Fight to your last breath!", "An endless", the digits, "-minute", "-man", "-life"). PAL shifts the two tables differently (`pc_region_sis_index`: all of `SdMenu` from 2, `SdSlChr` only from 28 to 74), which the fork follows as the game does; no PAL disc has been checked.
- **The rules header** is composed by `mnCharSel_8025BD30` from `GameRules` and those pieces: "2-minute KO fest!", "An endless KO fest!" without a time limit, "3-man survival test!" for stock, "1-life survival test!" for one stock, coin and bonus as time with their own last words, Camera mode "Take Snapshots!" and Stamina mode, which has no header to reach, "Fight to your last breath!". The digits are written as glyphs into the scratch line, which PAL moves to another number. The fork composes the same words from `GameRules` rather than decoding the scratch line, so they can be tested and do not depend on the region's layout.

### Online

- **Online lobby** (`src/melee/gm/gmonlinemode.c`, drawn by `src/melee/mn/mnonlinelobby.c`, base port): the first scene of its own mode, `GM_ONLINE`, so entering it is a scene change. Every frame `gmonlinemode.c` fills an `OnlineLobbyView` (`src/melee/gm/gmonlinemode.h`) with the lobby phase, the players found (name, ping, whether host), the rows, the highlighted row, a message, a hint and the keyboard for a friend's connect code, and a title and subtitle; its text is plain strings. The view is a clean place to read the whole screen from.

### Before a match

- **Character select** (`src/melee/mn/mncharsel.c`): free cursors. See [Character select in detail](#character-select-in-detail).
- **Stage select** (`src/melee/mn/mnstagesel.c`): one free cursor, the stage cursor, over a grid of 29 stage tiles and the random tile. The hovered tile is the global `mnStageSel_804D6CAE`, an index into the table `mnStageSel_803F06D0`, whose rows hold each tile's `StKind`. It starts at `0x1E`, meaning no tile, changes when the cursor enters any visible tile, and keeps the last tile while the cursor crosses empty space. Start picks a random stage only until the cursor first enters a tile: from then on A and Start both pick the last tile entered, even with the cursor over empty space, and only the random tile picks at random. The cursor starts at (0, -13), below every tile. It moves per axis and linearly, 0.03 units a frame for each step of the stick beyond the dead zone of 30, so at most 1.5 a frame, clamped to x ±27 and y ±19; its model's translation is in the same units the tiles are tested in. The frame function takes the dead zone off each axis before the cursor's update sees it (an axis of 50 arrives as 20, one within 30 as 0). With the cursor merged, the first port of the four whose stick is past 30 on either axis moves it, port 1 tested first, so another controller moves it only while port 1's stick rests. A step between neighbouring tiles takes about 6 frames at a full push, the longest glide across the grid under a second. The tiles' models slide in from the right as the screen opens, so a tile's position read on the first frames is not where it comes to rest. A locked stage in the main grid is still visible, a locked tile drawn as a "?" (its row's `x8` is 1; unlocked is 2), and A on it plays the refused sound, so a reader must not speak a locked stage's real name. Locked stages outside the main grid are hidden (`x8` is 0) and never hovered. Stage names and previews are pictures. The layout (see `docs/screenshots/stage-select.png`): two rows of stage pictures at the top (the first with one extra tile set apart at the far left); below them, the random tile on the left beside a row of five stages, and a row of "?" tiles under those five. Ranked online play replaces the grid with a text list of plain strings (`rankStage*` functions). Online, each player picks on their own, the picks are exchanged, and a coin flip on the session's seed chooses one of the two (`netStageSel_*` in the same file); the match waits until both have picked, so one player's Start leaves them waiting for the other.

Every mode's character and stage select is the same screen: the mode is a `CSSMatchType` value in the screen's data, and single-player types switch the screen from four cursors to one.

### During a match

- **Pause** (`src/melee/gm/gmvs.c`, overlay in `gmpause.c`): no menu. Start pauses and gives the pausing player a free camera; L+R+A+Start quits the match as a no-contest.
- **Training menu** (`src/melee/gm/gm_1884.c`): Training's pause, opened with Start during a Training match. A column of rows holding a value (Speed, Item, the CPUs, Camera) or an action (A on Item, Reset, Finish), and one hidden row the cursor skips. Labels and values are pictures; only the chosen item's name is game text. See [The Training menu in detail](#the-training-menu-in-detail).
- **Camera mode** (`src/melee/gm/gmcameramode.c`): the Special Melee match for taking snapshots. Before character select, a notice screen (`GS_CAMERA_VS`, `gmcamera.c`) explains the controls in game text (`SdVsCam`); the match itself is an ordinary `GS_VS` scene.

### After a match and between matches

- **Results** (`src/melee/gm/gmresult.c`, `gmresultplayer.c`): the summary table, then per-player pages of statistics, game text (`SdRst`). The screen moves on when every human player has pressed Start. See [Results in detail](#results-in-detail).
- **1P mode screens**: intro splashes (`SdIntro`), the Continue decision (`gmregclear.c`, `SdDec`), clear screens (`SdClr`), game over and "coming soon" scenes, "New Challenger" (`gmapproach.c`). Mostly sequences with one prompt.
- **Unlock notice** (`src/melee/if/ifprize.c`, `GS_PRIZE_INTERFACE`): shown after the title screen's Start, a VS or Special Melee match, a challenger fight or a single-player mode that unlocked something, one notice at a time; game text (`SdPrize`). See [Unlock notices and challengers in detail](#unlock-notices-and-challengers-in-detail).
- **Tournament** (`src/melee/gm/gmtou_0.c` for setup, `gmtou_1.c` for the bracket, `gmtou_2.c` for choices between rounds): cursor menus with game text. Large, only skimmed.
- **Staff roll** (`src/melee/gm/gmstaffroll.c`): a free cursor that shoots floating names during the credits; ends on its own.

### Trophies

The trophy screens' interaction lives in `src/melee/ty/` (`toy.c`, `tylist.c`, `tydisplay.c`, `tyfigupon.c`); the `gmtoy*.c` files in `gm/` are only mode shells. The gallery and collection show a trophy that the player rotates and zooms, with its name (`SdToy`) and description (`SdToyExp`) in game text; the lottery is a machine the player feeds coins. Only skimmed.

### Developer screens

`GS_DEBUG_MENU` (`src/melee/gm/gmdebugmode.c`) and `gmhanyucss.c` / `gmhanyusss.c`, which cycle character and stage select through every mode's variant, exist only for developers and are out of scope.

## Character select in detail

The most complex native menu, and the one no player can avoid. The layout (see `docs/screenshots/character-select.png`): the rules header ("2-minute KO fest!") across the top, the Back button in the top-right corner, three rows of character portraits in the middle, with locked characters shown as "?", and the four player slots along the bottom, each with its HMN/CPU tab, its name box and, for a CPU, its level slider.

- **All of the screen's state is private to `mncharsel.c`**: the hands, the coins, the doors, the portrait table, the mode and the flags are `static` there, and the hand's and coin's structs are defined only in that file. Nothing can be read from outside it, so hooks placed in `mncharsel.c` have to hand the state over, and a hand's own numbers can only be caught as that hand updates.
- **Doors** are the four player slots along the bottom (`CSSDoor`, `src/melee/mn/types.h`, array `mnCharSel_803F0DFC.doors`). Each holds the slot's player kind (`p_kind`: human, CPU or closed), `team`, `costume`, and `sel_icon`, the portrait this slot's coin rests on or was last over. Picking the coin up sets `sel_icon` to `0xD`, a placeholder (it is Kirby's portrait), until the coin's first frame in the hand; `0x19` is written only when the coin goes back into the slot. While a coin is carried, `sel_icon_prev` is the portrait it is over, or `0x19` over empty space; at rest it equals `sel_icon`. "Door" is the decomp's word; players say "player slot".
- **The chosen character** is `mnCharSel_804D6CB0->vs.start.players[i].ckind` (in single-player modes the player is the port that started the mode, not the door). Picking the coin up keeps it, which is what B puts the coin back on; carrying the coin down into the player slots clears it. A closed slot, as every slot is on first arrival, can hold a stale character with no coin on its portrait, so a character counts only while its coin rests on a portrait. The players' data sits after the match rules, whose bitfields the game's files lay out with `-mno-ms-bitfields` on Windows; port-layer code that reads it needs the same option, or every player is read from the wrong place (the fork's `a11y.cmake` gives it to `game_access.c`).
- **Cursors** are the hands, one per active port (`CSSCursorData`, local to `mncharsel.c`, every field still unnamed): `x4` is the port, `x5` the hand's state (free, carrying, or hidden while its controller is unplugged), `x6` what a carrying hand holds (a door's coin, or a door's CPU-level or handicap slider), `xC` and `x10` its position in menu units. An unplugged controller hides its hand and turns its player slot into a CPU.
- **Icons** are the portraits (`static CSSIcon icons[]` in `mncharsel.c`): a rectangle for hit testing, the character (`char_kind`, a `CharacterKind`), a locked/unlocked state and the chime sound. Unlike a locked stage, a locked portrait ("?") is never hovered: the hit test skips it. `SelectableCharacterKind` in `types.h` names the portrait order.
- **The coin**: picking a character means carrying a coin onto a portrait and pressing A. A carried coin sits 2.7 units right of and 2 below the hand, and hovering is tested at the coin, not at the hand. While the coin is over a portrait, the screen sets that door's `sel_icon` every frame; moving off a portrait leaves `sel_icon` unchanged. A commits the portrait, plays the chime and calls the announcer. A hand can carry another door's coin, which is how a human picks a character for a CPU.
- **Getting the coin into the hand**: a hand in the portrait band whose player has no character gets its coin automatically; A near a resting coin picks it up, if it is one's own or a CPU's; B anywhere in the portrait band calls one's own resting coin back. "Near" is measured from a point 3.8 units right of and 2.6 below the hand: the nearest such coin within 3 units of it, the lower door first when two are as near. The frame of the pickup still shows where the coin lay; where it is under the hand shows a frame later.
- **Where a resting coin is**: wherever it was dropped, or, when the game places it (a new CPU's random character, B putting it back), 3.4 units in from its portrait's left edge and 3 down from its top. Resting coins then drift a hundredth of a unit a frame away from each other and toward 1.5 units inside their portrait's edges. Only the coin's own update (`fn_80262648`) knows where it is, so a hook there hands the position over.
- **Letting go of the coin**: B while carrying puts it back on the character chosen before, with the chime and the announcer, if there was one. Carrying one's own coin down into the player slots returns it and clears the character; another's goes back to its earlier choice. Carried up into the top bar, the coin stays in the hand and the buttons there do not react: A plays the refused sound, and B still puts the coin back. With all 25 characters unlocked, the two empty corners of the bottom row are hidden targets where A chooses a random character (read from the code, not tried).
- **Other targets**: the HMN/CPU toggle and team button on each door, the CPU-level and handicap sliders, the name box (opens the name tag window, a list of saved names in game text), the rules header at the top centre (opens the rules screens inside character select), and the Back button in the top-right corner (holding B does the same). X and Y change the costume (`mnCharSel_CostumeChange`, one of the few named functions).
- **No hovered button.** The game keeps no record of which button a hand is on: `mnCharSel_CursorThink` tests the hand's position against each target only when A is pressed, in a fixed order (the top bar, then door by door its HMN/CPU button, team button and sliders, then the name box), so a reader repeats those tests. Only the top bar and single-player arrows set a flag (the hand's `x8`), and only to make the hand blink. A hand carrying a coin reacts to no button at all.
- **Where the buttons are.** The top bar is above y 22: Back right of x 17.3, the rules header between -17 and 15 (none in Stamina mode), Teams left of -25.5 (VS Melee and the Special Melee types up to Slo-Mo). Each door's HMN/CPU and team buttons are x ranges in the door's data (`togglebtn_*`, `teambtn_*`) with heights fixed in the code; the two sit side by side. The sliders' knobs and the name box are not in any table: the code finds them each time from the screen's model, by joint number (`lb_80011E24` from the model's root, then `lb_8000B1CC` for the joint's position, which sets up the joint's cached world matrix if it is out of date). A slider is grabbed within a squared distance of 5 of a point offset from its joint, and its joint moves with its value.
- **The HMN/CPU button** cycles human, CPU, closed, and skips human when that port's controller is unplugged. It does nothing while the door's coin or slider is in any hand, while the door's own hand holds something, or while its name tag window is open. A click moves the hand to a fixed height on the button, so the hand stays on it. A door made a CPU that has not chosen since the screen loaded, and is not the clicking hand's own, gets a random character at once. A hand's own closed door opens (as human) when the hand reaches the portrait band.
- **Teams and sliders.** In a team match every open door has a team button cycling three teams, drawn in the colours of ports 1, 2 and 4: red, blue, green. A CPU's door has a CPU-level slider; with the handicap rule on, every open door has a handicap slider too, which a player may set for their own door or a CPU's (with it on, or automatic, the CPU-level slider moves to a second joint). A held slider keeps the hand at its knob's height and follows the hand's x over 10 units (`updateGrabbedSlider`): the knob's place along them, times 0.8, plus 0.5, truncated, plus 1, is the value, 1 to 9. So each value but the two ends spans 1.25 units, the ends half that. The joint's own x (its translation) is that place, so the joint's world position less its translation gives the slider's lowest end.
- **The rules screens and name entry** open inside the character select scene: A on the rules header, or on the name tag window's row after the saved names (while the list has room), sets the pending exit (`mnCharSel_804D6CF6`) to 3 or 4, and the next frame frees the screen's gobjs, loads the other screen and sets it to 5. `mnCharSel_Scene_OnFrame` keeps running meanwhile, but the screen's model and name tag windows are gone, so character select's own state is then only its static tables; the rules screens keep theirs in `mn_804A04F0`, as in the main menu tree ([The rules screens on character select](#the-rules-screens-on-character-select)). Leaving builds character select afresh (`mnCharSel_802640A0`) in the same scene, with the hands at home: no scene change marks the return.
- **Ready to Fight** is a state, not a target: the banner appears (`mnCharSel_804D6CF7` nonzero) when every open slot has a character, enough players are in (at least two teams in a team match), no hand holds a coin or a slider and no name tag window is open; in single-player modes, once the one coin is placed. Start from any active controller then confirms the choices and leaves the screen; Start before that plays the refused sound.
- **Single-player modes** (the match types from Classic, `0xB`, up) share the portraits and the coin, around a different layout:
  - **One hand**, index 0, read from the controller that started the mode (`mnCharSel_804D6CF0`, from the screen data's `unk_0x0`). Its badge shows that port's number. The hand starts below the one player slot, left of the arrows.
  - **One player slot**, door 0, always human, and in Training a second, the CPU's (door 1, the player `mnCharSel_804D6CF1`). Its tab reads "CP", it has no HMN/CPU button, and a character the save does not have is replaced by a random one as the screen opens, so its coin is always on a portrait for the hand to pick up and carry. The doors after the mode's one or two keep whatever the last VS screen left, and so do the name tag windows' pointers, which by then point at freed memory: a reader must stop at the mode's slots.
  - **No Teams button and no rules header.** The top bar is Back alone.
  - **The arrows**, below the portraits, tested after Back, the stock arrows first: in Classic, Adventure and All-Star a row drawn "LEVEL", a strip of five difficulties ("VERY EASY" to "VERY HARD", game text drawn into the strip, values 0 to 4) between an arrow down and an arrow up; below it, in Classic and Adventure only, a row drawn "STOCK", stock icons 1 to 5 between the same two arrows. The difficulty and its arrows' rectangles are in `mnCharSel_803F0EBC` (the decomp calls the value a CPU level), the stock count and its arrows' rectangles in `data2`; both values are also copied into the starting player's `vs.start.players` entry. A on an arrow moves the value one step with the move sound; at the end of the range it does nothing at all, no refused sound. A hand carrying a coin presses no arrow.
  - **One name box**, its joint `tag_box_joint` in `mnCharSel_803F0EBC`, tested after the arrows.
  - **Ready to Fight** shows once door 0's coin rests on a portrait and the hand holds nothing; Training's CPU always has a character. X and Y change the costume while the coin is placed.
- **Multiple players**: in VS modes there are four cursors, one per controller, each independent; in single-player modes one. Online, the remote player's cursor moves on this screen too, driven by their synced controller, and the local controller always reads as port 1 while the local player can be any of the four, so their hand is the local player's (`pc_net_local_player()`), not port 1's.

Everything a sighted player reads here is a picture except numbers (rules header, CPU level, handicap, KO counts), timers, name tags and the character's name in the name tag window.

## The Training menu in detail

Training has no pause screen of its own: Start during a Training match opens the Training menu, a column of rows on the left over the frozen match, the hovered row drawn gold with arrows. The fork reads it in `training_speech.cpp`.

- **State.** All of it is in the global `gm_80473814` (a `CssSubStruct`, `gm/types.h`): `x00` the row the cursor is on, `x01` whether the menu is open, `menu_values[]` one value per row. The struct also keeps the players' starting data, whose bitfields need the game's layout, so it is read from a file built like the game's (`game_access.c`).
- **Open and closed.** `x01` is the menu's own record and the one to read. Its update, `fn_801891F4`, sets it on the first tick it runs with the pause word's Training bit set (bit 2, `gameplay-state.md`, "Pause, freezes and slow motion") and clears it on the tick that bit clears, applying the values; so it changes exactly once per Start. The panel's slide (`anim_frames`) is drawing only. Only the player who paused can unpause, and each of the two is locked out for 10 ticks after the other.
- **Input from the first tick.** The update reads the controller on every tick the menu is open, the first included, so a press while the panel is still sliding in already moves the cursor or changes a value. The fork's reader says such a press after its opening rather than in its place.
- **Persistence.** The scene's start (`fn_80189B88`, from `gm_Scene_Training_OnEnter`) puts the cursor on Speed and every value at its default. The cursor and the values then persist between openings for the rest of the scene. Finish ends the match at once and lands on Training character select; the next match enters the scene again, back at the defaults. The fork's scene hook runs right after that start, so `pc_a11y_scene_entered` with `GS_TRAINING` marks a new session.
- **One controller**: the human player's (the Training state's port, read through `gm_801A36C0`). The D-pad and the stick do the same. Up and Down move the cursor and wrap, Finish to Speed and back, skipping the hidden row. Left and Right change the value and wrap on every row; Reset and Finish ignore them. A on Item spawns the chosen item above the player, with the same sound whether or not the item limit let it spawn, and holding A spawns one after another. A on Reset puts every value back to its default and the player at the spawn point, leaving the cursor on Reset. A on Finish ends the match without confirmation. B and the other buttons do nothing; Start closes the menu.
- **Auto-repeat.** Every held button repeats (`gm_1A36.c`): the press at once, then after 20 ticks one every 8, from 40 ticks every 4, from 100 ticks every 2. So a held direction's first repeat comes a third of a second after the press, and the later ones about 0.13 s, then 0.07 s, then 0.03 s apart. The Training menu holds back a change that comes sooner than its settle time (`TrainingSpeech::kSettle`) after the last, so the settle time, set against these gaps, decides which repeats a held direction is heard as; the value it settles on is always said. CP Damage pauses the repeat 25 ticks at 0 and at 999 before it wraps, on controller 1 only.
- **When a change applies.** Camera applies at once. Speed, the number of CPUs, the CPU's behaviour and its damage apply on the tick the menu closes, every time it closes, so each pause sets the CPUs' percent back to the menu's value.
- **Slow motion slows the menu.** The chosen speed stays in force while the menu is open, and a tick the frame timer skips runs no proc, so at 1/4 the menu runs 15 times a second and its repeats come four times slower. The match's tick hook still runs on every tick, skipped or not.
- **Game text.** The chosen item's name is the menu's one text object, an SdTrain string chosen through the menu's own item table; on a US English game the Motion-Sensor Bomb gets another string. There is no pause notice, though the code looks as if there were one: `fn_80188EE8` binds SdTrain string 30 to that same text object while paused, then rebinds it to the item's name later in the same function, every tick.
- **Where the fork reads it.** The match's tick hook (`pc_a11y_match_tick`) runs after the tick's procs, paused ticks included, so it sees each tick's finished state, the menu's own update included; in the Training scene it hands the menu to the reader whether or not the cues are on. The menu needs no hook of its own.

The rows, by `x00`, with the values `menu_values[row]` keeps, in the order Right steps through them:

| Row | Drawn | Values | Default |
|---|---|---|---|
| 0 | Speed | an index into the speeds, fastest first: ×2, ×1.5, ×1, 2/3, 1/2, 1/4 (Right goes slower) | ×1 (2) |
| 1 | Item | 0 to 29, an index into `gmTraining_ItemTable` (static in `gm_1884.c`), whose entries pair an `ItemKind` with its SdTrain string; not an `ItemKind` itself | Food |
| 2 | No. of CPs | 0 to 2, drawn and applied as 1 to 3 | 1 |
| 3 | CP: | 0 to 5: Stand, Walk, Evade, Jump, Attack, Human | Stand |
| 4 | CP Damage | 0 to 999, drawn as the number | 0 |
| 5 | none | hidden: the cursor skips it, nothing draws or reads it | |
| 6 | Camera | 0 to 2: Normal, Zoom, Free | Normal |
| 7 | Reset | none; A only | |
| 8 | Finish | none; A only | |

The item table holds Item Switch's items but the Party Ball, in another order, and a 31st entry, no item, out of the cursor's reach. Since the table is static, the fork keeps a copy of its kinds (`training_names.cpp`) and names the items by `ItemKind` from one table shared with Item Switch (`item_names.cpp`), so an item is named the same on both screens.

## Results in detail

Results is one scene, `GS_RESULTS`, for VS, every Special Melee mode, Tournament, online play and the debug VS match. The announcer names the winner while the scene builds; then the summary table shows every player's place and a few numbers, and the first press from anyone opens a stats panel per player, which each player pages through and closes with Start. The fork reads it in `results_speech.cpp`, from a snapshot `game_access.c` takes once a tick (`a11y_game_results`).

### State and flow

- **Where it lives.** `lbl_8046DBE8` (`ResultsData`, `gm/types.h`), a global of `gmresult.c` declared in no header, holds the scene's state in `x1`, a sub-phase of state 3 in `x0_23`, the frames since entry in `x8`, the winner in `x4`, `x5` and `x6`, and per slot the panel's page and ready flag. `x94` points to the `MatchEnd` the match left, which holds everything the summary shows. The scene's entry zeroes the struct and sets `x94` before the fork's scene hook runs.
- **The states**, in order: 0, the announcer runs on a frame count (`fn_80179350`) and no press counts, except that on a cancelled match a human's Start skips it; 1, the summary table animates in, 40 frames; 2, the table is up and the first new press from any connected human opens every stats panel (`fn_80177920`), or from any connected controller when no human is playing (the four-CPU debug match, where controller 1 sits in P1's CPU port), so state 2 always lasts at least one tick; 3, the panels (`fn_80178050`); 4, everyone is ready, a short delay, and the scene leaves.
- **A cancelled match.** L+R+A+Start from the pause ends a match as a No Contest. Results then shows "NO CONTEST" in the banner, no place images, and the summary's usual rows (a Time match's Total among them); the panels have two pages, KOs and falls and Stats.
- **State 3's sub-phase.** `x0_23` is 0 while the panels slide open; on the tick their animation reaches its end it goes to 1 and at once to 2, so between ticks it reads 0 or 2. The panels take input from 2.
- **The opening's moment.** About 3.3 seconds after entry, the announcer having named the winner, the scene reaches state 2; on a No Contest, as soon as Start has skipped the wait. The fork says its opening on the first tick it sees state 2 or later and nothing before; the scene speech table leaves results silent.
- **Static values.** Two values the screen depends on are static and must be recomputed, never read: `lbl_8046E190`, the Bonuses page's row counts, and `lbl_804D3FC8` in `gmresultplayer.c`, whether no human is playing. `fn_80174284` must not be called: it records each bonus as a side effect.

### What the summary table shows

`MatchEnd.player_standings[i]` is port *i*. The fork recomputes each cell the way the drawing code does, clamps included, so the snapshot holds what the screen shows:

- **The match type**, SdRst strings 2 to 5 by `match_kind`: "Time Battle", "Stock Battle", "Coin Battle", "Bonus Battle" (`fn_80175DC8`).
- **KOs** (`x20`), **Falls** (`x24` less the self-destructs) and **SDs** (`self_destructs`), each clamped to 999 (`fn_80175240`). Bonus mode shows none of the three.
- **The labels' points.** Beside the KOs, Falls and SDs labels the summary draws the points each counts for ("+1", "-1", "-1"), in Stock matches too; the fork does not say them.
- **The last row** (`fn_80175A94`): Stock the KO time, `x28` frames as m:ss up to 99:59, or "-:-" for the player the banner names, a player still in, a winning-team player still in or knocked out on the match's last frame, or on a cancelled match a player still in or level with the winner (`fn_80175880`); Time the total, `score` clamped to 9999 and drawn with its sign; Coin the coins, `x1C`; Bonus the points, `score`, drawn as a dash on a cancelled match.
- **The character** comes from `ckind`, with Zelda and Sheik by the form the player ended in (`ftkind`), as the results model (`fn_8017A67C`) and the announcer (`fn_80168E54`) choose; the name is drawn as an image, so the fork names it from its own table.
- **Empty ports** show "NO ENTRANT"; the fork leaves them out.

### Winner and places

- **The banner.** Outside teams it names `x6`, which equals `x4`: the highest port whose `is_small_loser` is 0 (`fn_80165418`). In a team match it names the team `x5`, the last active team whose own `is_small_loser` is 0 (`fn_801654A0`), and `x6` is the player of that team with the lowest `is_small_loser` (`fn_801771C0`). A cancelled match shows "NO CONTEST".
- **Place images** (`fn_80177748`). Outside teams, the player with `is_small_loser` 0 gets the winner's image (1st with a crown) and everyone else the image of their `is_big_loser`, 0 for 1st to 3 for 4th; in a team match the player in `x6` gets the winner's image and everyone else their team's `is_big_loser`. A cancelled match shows no place.
- **Ties.** `is_big_loser` is the place, shared by tied players; `is_small_loser` is the game's order with ties broken, so it tells which tied player the banner names. The fork says players by place, the banner's player first among the tied, then by `is_small_loser` and port. Ties are common in the four-CPU debug match on Poké Floats, where several fighters often fall out on the same frame early in the match: three such players all show "2nd" with the winner 1st, or two show "2nd" and the next shows "4th".
- **Teams.** `player_standings[i].team` numbers the teams as character select does: 0 red, 1 blue, 2 green; the summary's colours agree (`fn_8017507C`). The screen shows a player's team only as the colour of their column's numbers, and the banner names the winning team ("BLUE TEAM").

### Game text

Font slot 0 holds `SIS_ResultData` (SdRst), 317 strings on an NTSC-U disc, all decoding, none malformed. The fork reads it by number through `results_string` in `game_access.c`, which checks the symbol, as `menu_string` does for the menus.

- **Match types**: 2 to 5, as above; 0 and 1 are empty, 6 and 7 Japanese leftovers in font glyphs.
- **Stat labels**: 8 to 55, the label of each row of the stats lists (`lbl_803D6488` and `lbl_803D6558` in `gmresult.c`) by the entry's number. They are drawn without spaces ("DamageGiven", "HitPercentage", "TopSpeed"); the empty strings between them belong to the rows that carry a value. Two labels span two strings, one per row: "Attempted" and "Grabs" (38, 39), "Transformation" and "Time" (54, 55).
- **Bonus names**: from 60 ("First Place", "Last Place", "Wire to Wire", ...); the code adds 60 to the bonus's number (`fn_80174468`). 56 to 59 are "Friendly Fire", "Coin Points", "Swiped Coins" and "Lost Coins". One font glyph, 0x4000, is the é of "Pokémon KO" (285).
- **No units.** Neither "%" nor a distance unit is among the strings, and the values are drawn without one ([The stats panels](#the-stats-panels)).
- **PAL.** `pc_region_sis_index` has no shift for this table; whether the PAL table numbers its strings the same is unchecked, for want of a PAL disc.

### Name tags

Page 0's opponent rows name a player by their tag where they have one (`fn_8017BB94`): `player_standings[i].x4` is the tag's slot, 0x78 for none, which draws "Ｐ２" (full-width, on a US language setting) and otherwise `GetPersistentNameData(slot)->namedata`, a Shift-JIS C string formatted into the text at runtime. The game is compiled with CP932 as its execution character set, so its own literals ("Ｐ２") are Shift-JIS too. `decode_shift_jis` in `game_text.cpp` reads a tag: plain ASCII, full-width letters, digits and the name keyboard's symbols (`mnnamenew.c`) folded to ASCII, kana as kana; anything else is left out and logged. The fork names the opponent by the tag in place of the port and adds the character: "MA Mario, 1".

### The stats panels

Each port's panel lists one page at a time, ten rows tall, scrolled by the stick. The lists are tables in `gmresult.c`: `lbl_803D6878` names one per page, `lbl_803D6488` (KOs and falls) and `lbl_803D6558` (Stats) hold their entries, and the Bonuses page has no entries, only a count per slot in `lbl_8046E190`. `fn_80174B4C` builds the visible rows, `fn_80174468` fills each, `fn_801743C4` decides whether an entry shows at all.

- **The pages.** 0, KOs and falls: "KOs", a row per other player with the KOs the panel's player dealt them, "Falls", the same players with the times each KO'd the panel's player, and "SDs" with its count on the same row. A player is drawn as "Ｐ２" or their name tag ([Name tags](#name-tags)); every other player in the match has a row, CPUs included. 1, Stats: 23 stats, each a label row and a value row below it, right-aligned. 2, Bonuses: each bonus the player earned, a name row and a points row; on entry the page's count is 2 + 2 per bonus (`gm_Scene_Results_OnEnter`, through `fn_80174284`), or 0 while `fn_801701B8` is set (the debug menu sets it), and a player with no bonus gets an empty page. Bonus mode opens every panel on page 2; a cancelled match has 2 pages.
- **Values.** Every value is drawn with `"%d"` and no unit: damage without "%", distances as a bare number (feet on a US saved language, `x5C / 30.4788`, else `/ 100`), times as whole seconds (Ground time 19 after a 20-second match). The value functions (`fn_8017AE0C` to `fn_8017BB30`, `gm_17AD.c` and `gm_17BA.c`) clamp to ±999999 and only read the `MatchEnd`, so the fork calls them; a negative value is drawn "---" (`lbl_804D3F8C`), which none of the stats here has shown. Bonus points are drawn with a minus sign when negative. Fields with no agreed meaning (`x40` to `x4C`, `x8C`, `x90`) are simply the numbers their labels head.
- **The hidden first row and the never-drawn last.** Every list's entry 0 duplicates entry 1 (the Bonuses page's is a lone "−" value row), and `fn_80174B4C` places the first entry it draws one row above the panel's top edge, where the scissor (`fn_801749B8`) clips it: it is never seen. The loop also stops before the list's last entry, a duplicate of the one before it. The snapshot builds rows from what they mean, not from the entries, so neither reaches the reader.
- **The bonus rows.** The k-th earned bonus is the k-th nonzero flag of `MatchEnd.x44C[slot].x0[0..255]`; its name is SdRst `(u16) fn_8016F280(fn_8016F1F0(i)) + 60` and its points `x44C[slot].x104[i]`. The fork counts them again rather than call `fn_80174284` or read `lbl_8046E190`.
- **Paging** (`fn_80177B7C`): A or the stick pushed right goes forward, B or the stick pushed left back, with the move sound and the scroll reset; at either end nothing happens and no sound plays. The stick up and down scrolls by fractions of a row. A CPU's panel pages and scrolls by itself (`fn_80177DD0`), unless a controller is plugged into its port: then that controller pages it, so in the debug match with four CPUs controller 1 pages P1's panel.
- **The first press only opens.** In state 2, `fn_80177920` only sets state 3; the panels take input from `fn_80178050` once `x0_23` reaches 2, after the slide, and a press is new on its own tick alone. A first press of A opens the panels on page 0.
- **The D-pad, Z and the rest** are read by no code of the panels; Start readies and un-readies ([State and flow](#state-and-flow)).

### Labels and words

The stat labels come from SdRst ([Game text](#game-text)). The fork joins a label drawn over two rows ("Attempted", "Grabs") with a space, puts a space before each capital that follows a lower-case letter and lower-cases the word it starts, unless the word is all capitals: "DamageGiven" is said "Damage given", "Attempted Grabs" "Attempted grabs", "KOs" stays "KOs". Every label of the 23 reads right that way. Bonus names are said as written ("Run, Don't Walk"). The headings "KOs", "Falls" and "SDs" are SdRst 8, 9 and 12. The page names ("KOs and falls", "Stats", "Bonuses") are the fork's: the screen shows no title. The fork says a value as the screen draws it, the number alone, "none" for "---".

### The reading cursor's input

The reader sees the local port's newly pressed D-pad directions and Z in `HSD_PadCopyStatus[port].trigger`, read after the tick, the same pad the panels read; online that pad holds the local player's synced input, which the D-pad reaches whatever quick chat's mask does ([netplay primer](netplay.md), "Quick chat shares the D-pad"). The press that opens the panels is taken as only that: the reader reads no row on the tick it sees state 3 first. Since anyone's press opens them ([State and flow](#state-and-flow)), the reader also sees whether the local port pressed anything on that tick (any bit of `trigger`, the game's own test): only your own press interrupts what is being said, and the panels' opening after another player's press is queued behind it.

### Ready

- **Start toggles it.** Once the panels take input, a human's Start sets their panel's ready flag (`player_data[i].x0_0`, in `fn_80178050`) with the confirm sound and closes the panel to "READY FOR THE NEXT BATTLE"; Start again clears it with the back sound and brings the panel back on the same page. A ready panel takes no paging, but its data stays, so the fork's reading cursor goes on reading it.
- **Who is ready without a press.** On the tick the panels first take input, CPUs and empty slots are set ready, and so is a human whose port's pad reports an error (`HSD_PadCopyStatus[i].err`: no controller plugged in), which the game checks again every tick. Offline a second human has no controller, so readying is one player's alone. Online both players' pads read as plugged in (`from_wire` in `net_wire.c`), and ports 3 and 4 are empty.
- **No human.** When no human is playing (`lbl_804D3FC8`), the game holds every CPU's ready flag clear instead, and Start from any connected controller on a CPU's or an empty port ends results at once, with the confirm sound. The fork says nothing of it, as it says nothing of CPUs.
- **The end.** On the tick every flag is set the scene goes to state 4, and after a short delay it leaves.
- **What the fork says.** The reader compares the flags tick to tick from the panels' opening and speaks only of humans with a controller: your ready names every human not yet ready, and another human's ready or un-ready is queued behind current speech. A ready that ends the scene says nothing, yours or theirs: the next screen speaks.

### Online and leaving

- **Each copy reads its own player.** Both copies simulate the same results; each reader follows its local player's panel and calls the other human "your opponent". Results runs in lockstep, so no frame is re-run and nothing is said twice.
- **Quick chat** gives up the D-pad to the reading cursor while speech is on ([netplay primer](netplay.md), "Quick chat shares the D-pad").
- **Leaving.** An announcement handed to the screen reader can only be cut off by a later one that interrupts, so the reader queues nothing as the scene leaves: a ready that ends results is silent, and every screen after results opens with an interrupting announcement (character select's opening, a scene's name, the lobby's return), which drops anything still queued.
- **A lost connection.** In a lobby session the remaining copy leaves results for the lobby within a tick of the peer leaving, and the lobby's reader says "Connection lost." with its opening ([netplay primer](netplay.md), "LAN play"). A session started at boot (the drivers' `MELEE_NET`) has no lobby to return to: the copy stays on results offline, the lost player's port reads as unplugged, the game readies it, and the reader says nothing of it.

### Where the fork reads it

The match's tick hook (`pc_a11y_match_tick`, [gameplay primer](gameplay-state.md), "The tick hook") hands results to the reader behind the usual speech gate, after the scene's own update; the scene hook marks each visit. "You" are controller 1's port offline and `pc_net_local_player()` online, and nobody when that port holds a CPU or nobody, as in the debug match with four CPUs. The reader follows the panel of that port, a CPU's included, which controller 1 then pages, or the first player's when the port is empty; the reading cursor starts on you, or on the first player by port when nobody is you.

## Memory card prompts in detail

The memory card scene, `GS_MEMCARD` (`gmscmemcard.c`), checks the card and shows a prompt in a message window (`gm_1ADD.c`) when the save cannot simply be loaded; a valid save loads with nothing shown and the opening movie starts. It runs at startup (`GM_BOOT`) and later as `GM_MEMCARD`, at a mode change after an autosave failed or the card folder appeared or disappeared. The fork reads it in `memcard_speech.cpp`, from two hooks in `gmscmemcard.c`.

### The card on PC

- **A folder of save files.** aurora emulates slot A as `<user dir>/USA/Card A/`, the user dir being `pc_get_pref_path()` (`%APPDATA%\melee-pc`, or `MELEE_USER_DIR`). A fresh user dir gets an empty, formatted card, so a new player meets "has no saved Game Data". The US save is `01-GALE-SuperSmashBros0110290334.gci`, 11 blocks.
- **No card.** `--no-card` (`aurora_card_set_present(false)`, `src/pc/main.c`) means none at all: "There is no Memory Card in Slot A.", and `gmmain_lib.c` then builds a save with everything unlocked.
- **Reaching it.** `MELEE_BOOT_SCENE` skips `GM_BOOT` and so the scene; a run that wants it starts with no boot scene (`drive.py --scene ""`, the default with `--card`).
- **A corrupted save.** Spoiling one data block is not enough: the game loaded a fresh save with byte 0x100 flipped in its first data block, its banner block, or several single blocks. Flipping byte 0x100 of every data block but the banner (blocks 1 to 10, file offset `0x40 + n × 0x2000 + 0x100`) gave the corrupted prompt; `drive.py --corrupt` does that to the save it copies in.
- **Noise.** Creating the save logs an aurora card error, a file it failed to close; the save is written whole and loads.

### State and the prompt function

- **State** is the file-static `enter_data`: `.decision` the scene's state, `.unk1C` the hovered choice (0 the left, Yes or OK; 1 the right, No or Try Again), `.unk0.unk0` 0 at startup and 1 in `GM_MEMCARD`.
- **Every prompt** goes through `gm_801AEE6C(kind, text_id, line_count)`: kind 0 a message, A or Start continues; kind 1 Yes and No (strings 3 and 4); kind 2 OK and Try Again (strings 2 and 5). A prompt with a choice starts on the left one.
- **Moved defaults.** `gm_801AF250` picks the prompt from the card check's result and, for some (Overwrite, Corrupted, Format), moves the hovered choice to the right just after `gm_801AEE6C` returns, in the same frame.
- **Input.** Left and Right, on the D-pad or stick of any controller, move the choice with the move sound, stopping at the ends without a sound; A or Start confirms with the confirm sound; B does nothing.
- **One-frame prompts.** The card calls are synchronous on PC, so "Erasing the corrupted data..." and "Creating new Game Data." are replaced on the next frame and never seen.

### Game text

Font slot 3 holds `SIS_MessageData` (`SdMsgBox.usd`, loaded by `gm_801AEBB0`), 25 strings on an NTSC-U disc, all decoding. The fork reads it by number through `a11y_game_memcard_string` in `game_access.c`, which checks the symbol; `pc_region_sis_index` has no PAL shift for it, unchecked. " / " marks the screen's line breaks:

| Id | Text | Kind | Reachable on PC |
|---|---|---|---|
| 2 to 5 | OK, Yes, No, Try Again | choice names | |
| 6 | The Memory Card in Slot A / has saved Game Data. / Overwrite this Game Data? | Yes/No, No hovered | after a failed autosave |
| 7 | The Memory Card in Slot A / has saved Game Data. / Load from this Memory Card? | Yes/No | practically never |
| 8 | The Game Data on the Memory Card / in Slot A is corrupted. Erase this / and create new Game Data? | Yes/No, No hovered | a corrupted save |
| 9 | Erasing the corrupted data... / Do not touch the Memory Card / or POWER Button. | message | one frame |
| 0xA | The Memory Card in Slot A / has no saved Game Data. / Create Game Data? | Yes/No | a fresh install |
| 0xB | Creating new Game Data. / Do not touch the Memory Card / or POWER Button ! | message | one frame |
| 0xC | Game Data has been created. / This game saves data automatically / while you play. Please do not / touch the Memory Card or POWER / Button while saving or copying. | message | after Yes on 8 or 0xA |
| 0xD | Game Data could not / be created. | message | a disk write error |
| 0xE to 0x15 | not enough space, no more files, must be formatted, formatted, could not format, could not be accessed, incompatible, damaged | mixed | practically never |
| 0x16 | There is no Memory Card / in Slot A. | message | `--no-card` |
| 0x17 | Continue without saving? | OK/Try Again | after a failed autosave |
| 0x18 | Continue without saving / or loading Game Data? | OK/Try Again | after No |

### Flow

- Create Game Data? → Yes → Game Data has been created → the opening movie.
- Create Game Data? → No → Continue without saving or loading Game Data? → OK → the opening movie, no save written; or Try Again → Create Game Data? again.
- No Memory Card → Continue without saving or loading Game Data? → Try Again → No Memory Card again.
- Corrupted → Yes → (Erasing, Creating) → Game Data has been created → the opening movie.

### Where the fork reads it

`pc_a11y_memcard_prompt(kind, text_id)` at the start of `gm_801AEE6C` records the prompt; `pc_a11y_memcard_frame(hovered_choice)` at the top of `gm_Scene_MemCard_OnFrame`, before the frame reads input, passes `enter_data.unk1C`. A prompt is said on the second frame start after the one that showed it: by then it has stayed up through a whole frame, which the one-frame prompts never do, and the default the game moved is the choice read. The scene hook marks each visit, so a frame before the visit's first prompt says nothing. The scene speech table leaves the scene silent: its first prompt is its opening.

## Unlock notices and challengers in detail

The unlock notice scene, `GS_PRIZE_INTERFACE` (`src/melee/if/ifprize.c`), shows what the player has just earned, one notice at a time, each in game text. The fork reads it in `unlock_speech.cpp`, from one hook in `ifprize.c`. A locked character comes first as a challenger, a fight that unlocks it when won; when one comes depends on the save: "The save file" gives its layout, and "Challengers and their thresholds" when each comes. The spec is `.scratch/unlock-notices/spec.md`.

### The unlock notice scene

- **Where it comes.** After the title screen's Start, after VS and Special Melee results, after a challenger fight won, and after the single-player modes: wherever an unlock is recorded and not yet shown.
- **One notice at a time** in a frame: a message (game text), the date and time the unlock was recorded, a category icon (a D-pad for a mode, sliders for a rule, a note for Sound Test, 全 for all characters or stages, a trophy) and one of three jingles. The date and icon are left unsaid: the message carries what was earned.
- **The queue** is built before the scene starts (`gm_ModeState_Prize_OnEnter`, `gm_1BFA.c`): every unlock not yet shown, in ascending order of its unlock number, then each newly got trophy, then any unlocks those caused. It has no limit and the screen shows no count. Each notice shown is marked seen in the save, so it is shown once.
- **Input.** About 6 frames after entry the first notice appears. A or Start on any controller shows the next; there is no minimum display time. After the last, a short closing animation, then the scene ends and the next one names itself.
- **Every agent run passes through 8 notices** after the title screen's Start: with no memory card the base port builds a save with everything unlocked and nothing seen (`gmmain_lib.c`). In order: All-Star, Random Stage Select, Sound Test, all characters, all stages, then the trophies Kirby Hat 4, Kirby Hat 5 and Marin.

### Notice text

- **The table.** `SdPrize.usd` (`SIS_PrizeData`, `SdPrize.dat` in Japanese), loaded into font slot 2 as the scene enters: 393 strings on an NTSC-U disc, all decoding. `pc_region_sis_index` has no PAL shift for it, unchecked. `MELEE_A11Y_TEXT_DUMP=1` dumps it at the first notice.
- **Which string.** `un_802FE918(kind, trophy_id, timestamp)` sets up each notice, the first and each next. It maps the unlock number through `un_803F9B30` to a string number, the unlock number plus 2 for numbers 0 to 0x3D, and binds it to the frame's message (`un_803F9D48.x6`). The unlock numbers run 0 to 0x41: the 11 characters, 11 stages, All-Star, Score Display, Random Stage, Mew and Celebi seen, Sound Test, all characters, all stages, mode clears, event counts, Target Test, 100-Man and 15-Minute Melee, Very Hard clears, VS match counts, trophy counts, a trophy, two special trophies and Event 51.
- **A trophy notice** (unlock number 0x3E) is composed at run time into the placeholder string 0x4A, one of four runs of お sized as buffers: string 65 ("You got / the "), the trophy's name (its id plus 79 on a US setup, `Toy_803063D4`), then string 66 (" trophy!"), with `HSD_SisLib_803A6530` and `HSD_SisLib_803A660C`, which index the table directly. Read after the composition, string 0x4A is the whole message, confirmed in a driven run.
- **Font glyphs.** The table's own font has three glyphs past the atlas: 0x4000, the é of "Pokémon" (Poké Floats, Poké Ball, Pokémon Stadium); 0x4002, the numeral II of the trophy Ray Mk II; and 0x4001, filling twelve three-glyph strings of unknown use. The decoder is handed the first two for this table alone (`prize_font_glyphs` in `game_text.cpp`); the same codes may mean other characters in another screen's font.
- **The disc's spelling** is kept as the screen shows it: "Mushroom Kingdom ll" with two lowercase Ls, and the double and trailing spaces, which speech collapses.

### Where the fork reads the notices

`pc_a11y_unlock_notice(text_id)` at the end of `un_802FE918` passes the string bound to the notice, 0x4A for a trophy, after the composition and the frame's other text are set. `a11y_game_prize_string` in `game_access.c` reads it by number from font slot 2, checking the symbol. Each notice is said as it comes, interrupting the one before, and the first of a visit adds the key; the scene hook marks each visit, and the scene speech table leaves the scene silent, since its first notice is its opening. Nothing is said as the scene closes. Netplay never reaches the scene: the online mode ends at results.

### The save file

What unlocks a character, and so brings a challenger, is in the save, which a test edits to bring one at once (`tools/a11y/make_save.py`, below). From `src/sysdolphin/baselib/card.c` and `crypt.c`, `src/melee/lb/lbcardgame.c` and `gm/types.h`, confirmed by edited saves that load:

- **The file.** The US save (`01-GALE-SuperSmashBros0110290334.gci`, "The card on PC") is a GCI file: a 0x40-byte directory entry (big-endian; the game code, maker, file name, and the block count at 0x38), then 11 sectors of 0x2000 bytes.
- **Sector 0** holds the comment, banner and icons: the game's own pictures, so **no save file is ever committed**, a test's included.
- **Each other sector is one block**: a 0x20-byte header (a 16-byte checksum, the block id as a big-endian u16 at 0x10, a sequence number, a file table), then the payload. `HSD_Encrypt` seals a block from byte 16 on: the checksum, a running byte sum, of the rest into bytes 0 to 15, then each byte scrambled with the scrambled byte before it as its key. `HSD_Decrypt` rejects a block whose checksum fails.
- **Block id 1** holds `GmSaveData` (0x1790 bytes) at payload offset 0, in two copies on a fresh save (sectors 1 and 10), in host byte order: little-endian on PC, unlike the console's. Offset 0x00 is the unlocked characters' bits (u16, one per character that can be unlocked, numbered as `lbl_803B78C8` in `gm_1601.c` numbers them), 0x08 the VS matches played (u32), 0x0C the VS play time in seconds (u32), 0x14 the characters played in VS (bits per `SelectableCharacterKind`).
- **The save tool** ports the cipher. A port that stopped matching the game's would find no block whose checksum holds, and the tool then refuses the file rather than write one the game would call corrupted.

### Challengers and their thresholds

- **Only while a character is locked.** Each of the 11 comes in VS once enough matches are played, and some by other ways too (below); VS is the way a test reaches.
- **After every VS match with a human**, a match quit from the pause included, `gmVsMelee_ExitResults` (`gmvsmelee.c`) looks for a challenger in turn, and the first found comes: by the VS match count (`gm_80172DD4`: the locked character with the lowest threshold at or below the count), then Mewtwo by VS play time (`gm_80172D78`, at 72000 seconds, 20 hours), then Marth once all 14 starting characters have been played in VS (`gm_80172E74`).
- **The thresholds** (`lbl_803B7AD0`, `gm_16F1.c`), in VS matches played: Jigglypuff 50, Dr. Mario 100, Pichu 200, Falco 300, Marth 400, Young Link 500, Ganondorf 600, Mewtwo 700, Luigi 800, Roy 900, Mr. Game & Watch 1000.
- **A quit match is not counted** (`gm_WasMatchCanceled` skips the count), so a save with every other character unlocked and the count at one's threshold brings that one after every VS match, quit or played to the end, run after run, until it is beaten. `make_save.py challenger NAME` makes that save from a fresh one, and `tools/a11y/challenger.drive` plays the quick VS match to the New challenger screen.
- **Single-player modes and events** bring challengers by conditions of their own (`fn_80173098`, `gm_801732D8` in `gm_16F1.c`), not looked into.

## Candidate hook points

Speculative: places the code offers, not decisions.

- **Scene changed**: `gm_801A4014` in `gm_1A3F.c`, right after the scene's enter function. One hook sees every scene change in the game, with the scene's kind, and can announce the scene or hand over to a screen-specific reader.
- **Main menu tree, tree screens**: the tree screens in `mnmain.c` keep the current screen and highlighted entry in the global `mn_804A04F0` (`MenuFlow`, `src/melee/mn/mnmain.h`: `cur_menu`, `prev_menu`, `hovered_selection`). There is no single function for "selection moved" or "screen changed": each tree screen has its own think function that writes `cur_menu` inline both ways, and `mn_80229894` is what leaf screens call to return to the tree (and what the base port uses to open Online). Polling `cur_menu` and `hovered_selection` once a frame catches all of it; this is the case the isolation rule allows polling for, and the fork polls once a frame from the menu scene's own frame function, which runs before the menu's objects update. The poll is scoped to `GS_MENU`; inside `GS_CSS` the rules screens are read by the same reader fed from character select's frame hook, and the name keyboard is unread. The alternatives, and why the poll won, are in `docs/adr/0003-main-menu-tree-is-polled.md`:
  - two choke points in `mnmain.c`: `mn_8022B3A0` builds every tree screen, and the selection-changed branch of `fn_8022AFEC` sees every hover change. They cover the tree screens exactly, but no leaf screen passes through either, so every leaf screen would need its own hook;
  - the description line (`mn_80229A7C`), which misses Online and moves made during a slide (see [Decoding game text](#decoding-game-text)).
- **Main menu tree, leaf screens**: the screens at the ends of the tree need per-screen readers, because where they keep their state varies:
  - Custom Rules and Additional Rules keep the row in `hovered_selection` and the hovered row's value in `confirmed_selection`, which the input function sets; the screen's own update copies every value into `GameRules` on the frame it changes (`fn_802309F0`, `fn_80232F44`), so a poll the next frame reads the rules there, the same `u8` fields the match uses. Additional Rules' cursor skips the rows `mnRulePlus_IsOptionVisible` hides, so a reader keyed by `hovered_selection` meets only the rows drawn. Neither screen uses `Menu_InitCenterText`;
  - the item and stage switches, the name list, the name keyboard and the records table use `hovered_selection` too (the records table packs row and column into it), with a value in `confirmed_selection` or their own struct;
  - Sound, Screen display, Language and Multi-Man keep their cursor in the generic `Menu` struct, private to their file. All four set the text in the middle of the screen through one shared helper, `Menu_InitCenterText` (`src/melee/mn/inlines.h`), each time the row or choice changes and once as the screen opens. The number of that text names the row or choice, so a hook in the helper (`pc_a11y_menu_center_text`) stands in for reading the private cursor; Screen display and Language have one row and set theirs once. The opening function sets `cur_menu` and the centre text in the same frame, so the poll sees both together; the fork keeps the last number across scenes, since after a Multi-Man match the menu scene opens straight on that screen before the scene hook runs. The values need no hook: each press saves them at once, Sound's channel as the synth's sound mode (`HSD_SynthGetSoundMode`, 1 for stereo; `lbAudioAx_80024BD0` reads the same but also writes a copy) and its balance and Screen display's deflicker in the game preferences (`gmMainLib_8015ED74`, -100 to +100 in steps of 5; `gmMainLib_8015F4E8`). The balance runs the opposite way to how its bar looks: Right moves the divider toward the MUSIC label, but it grows the SOUNDS part of the bar, and +100 silences the music. Screen display ignores A until its slide-in animation ends;
  - every other screen keeps its own state (Rumble in `MnVibrationData`; event match, the other records pages, the sound test, the snapshot album, the movie gallery, Erase data).
- **Something happened**: `lbAudioAx_80024030`, the menu sound function. One hook there tells the fork the player moved, confirmed, cancelled or was refused, on almost every native menu, including screens outside `mn/`. It carries no context, but it tells a poller when to look, and the refused sound explains a press that changed nothing.
- **Training menu**: polled from the match's tick hook, which needs nothing new (see [The Training menu in detail](#the-training-menu-in-detail)).
- **Results**: polled from the same tick hook (see [Results in detail](#results-in-detail)).
- **Character select**: the fork polls once a frame, as for the main menu tree, through the frame hook, with each hand's and each coin's numbers caught as they update. A carried coin entering or leaving a portrait shows in `doors[i].sel_icon_prev` together with the hand's state (`sel_icon` alone misses leaving and re-entering the same one, and holds the placeholder after a pickup); a pickup, a choice, B and the coin carried down show in the hand's state and the door's `sel_icon` and character. Changes of `p_kind` (human, CPU or closed) and `mnCharSel_804D6CF7` going from zero to nonzero (Ready to Fight shown; while shown it counts animation frames) are read the same way. The confirm path that calls the announcer is no hook point: hook the character select call sites, not `gm_80168C5C` itself, which other screens call too.
- **Stage select**: the fork polls once a frame through a hook at the top of `mnStageSel_Scene_OnFrame`, which hands over the screen's private state: the table of tiles, `mnStageSel_804D6CAE`, the pick state and the buttons the screen read. The pick runs in an object's update after the frame function, before the cursor's own update, so the buttons a poll sees acted on the tile the previous poll saw hovered. The tile table's row type is private to `mnstagesel.c`, so the fork keeps a copy of its layout and the hook passes the row's size to check it against. Steering's glide advances in a second hook, in the cursor's update after its clamp, which passes the cursor's position and the stick that moved it; that update returns early once a pick is made, so the hook falls silent there. Online, the frame function resolves the two players' picks once both are known and leaves on that same frame, after the frame hook has run, so a third hook at the resolution passes the stage's tile and both picks.
- **Game text**: the three functions in [Game text](#game-text) see most text as it is set. Whether a generic text reader built on them is useful, or too noisy without knowing which screen a text belongs to, is open.
- **Online lobby**: the `OnlineLobbyView` that `gmonlinemode.c` fills each frame.

## Design constraints

- **Many players, one menu.** Shared menus accept any controller, character select has one cursor per controller, and online a second human moves on the same screens. Whose actions to announce is a decision to make.
- **Free cursors need more than "read the item".** On character and stage select the cursor slides between targets over empty space; announcing the target the cursor enters is the obvious start, but a player also needs to find targets.
- **Steer only through the controller.** Online, character and stage select run in lockstep: both machines advance frame by frame on the same synced controller input, and rollback (re-running recent frames when a late input arrives) is switched on only once the match starts (`docs/netcode-plan.md`, native menu integration). A fork feature that changed menu state would desynchronise the peers. Feeding input is safe where it enters as controller input, before the game and the netplay code sample it: the peer receives it like any player's, and cannot tell it from one. It is unsafe anywhere later. Reading state and speaking is safe; the netplay speech gate (`pc_net_resim()`) costs nothing in menus.
- **Names can move.** Hooks go into decomp-layer files, which decomp syncs replace. Hooking named functions and reading named fields is sturdier than reading `x5`-style fields, and every hook of this kind is a merge risk worth keeping small.
- **Region.** String numbers are the NTSC-U build's, remapped for PAL discs by `pc_region_sis_index()`. String tables keyed by game identifiers (menu kind and selection, character, stage) are unaffected.

## For discussion

This section is a snapshot for the discussions ahead, not a description of the code; later work supersedes it.

### Where screenshots would help

Code gives the logic but not the layout. `docs/screenshots/` already covers the title, main menu, VS mode submenu, character select and stage select, and `docs/screenshots/macos-pal/` adds results and trophies (a PAL disc). Still missing, as screenshots or a sighted helper's description:

- how far apart character select and stage select targets are in cursor movement, and how the cursor feels to steer;
- the item switch and random stage switch grids;
- the name entry keyboard layout;
- the snapshot album and the trophy gallery in detail.

### Candidate vocabulary

Terms this document uses that may deserve a place in `CONTEXT.md`:

- **Menu screen**: one screen of the main menu tree, a `MenuKind` (the tree is one scene holding many screens).
- **Hovered entry** versus **chosen entry**: what the cursor is on versus what the player confirmed.
- **Tree menu**, **setting row**, **toggle grid**, **free-cursor screen**, **prompt**: the kinds above.
- **Picture label**, **value** and **game text**: the three sources of what a sighted player reads.
- **Player slot** (the decomp's door) and **coin** on character select.

### Open questions

- Does decoding game text by number work for all English strings? For the main menu's table it does (see [Decoding game text](#decoding-game-text)); results' `SdRst` decodes too ([Results in detail](#results-in-detail)); still to check are the other archives (character select, training, trophies, message windows), their font glyphs, and a PAL disc.
- Where a string table entry is patched at runtime, is its number still a usable key?
- How much of the trophy, snapshot and tournament code is worth reading before those screens are in scope?
