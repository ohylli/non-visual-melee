# Unlock notices and the New challenger screen: speech

Status: agreed 2026-10-29 (design conversation with the maintainer)

The maintainer agreed:

- The scope: every unlock notice, trophy notices included, and the New challenger screen.
- What a notice says, and that the date, the icon and a count are left out.
- What the New challenger screen says, the challenger named.
- A save tool that brings a challenger after one VS match, for agent runs and for the maintainer's own testing, rather than a boot scene.

The wording is a first proposal for the play test to judge.

## Goal

A blind player hears what they have just unlocked, in the game's own words, as each notice comes up, and knows to press A for the next. When a challenger appears, they hear who it is and that A starts the fight.

Vocabulary is `CONTEXT.md`: unlock notice, challenger, New challenger screen, scene, announcement, player label. How the game's menus run and its game text are in `docs/a11y/native-menus.md`.

## Scope

In:

- **The unlock notice scene, `GS_PRIZE_INTERFACE`** (`src/melee/if/ifprize.c`), wherever it is reached: after the title screen's Start, after VS and Special Melee results, after a challenger fight won, and after the single-player modes.
- **The New challenger screen, `GS_APPROACH`** (`src/melee/gm/gmapproach.c`).
- **A save tool** that makes a challenger come after one VS match, a drive script that uses it, and a one-command launch for the maintainer.

Out:

- **The challenger fight**: an ordinary one-stock match, already served by the match cues and match announcements. A loss goes straight back to the previous screen (VS: character select), which names itself.
- **The 1P trophy fall and Congratulations** (`GS_REGEND_TOYFALL`, `GS_REGEND_CONGRATS`): different scenes, a later feature.
- **Online play**: the online mode ends at results, so it never reaches either scene.
- **Disc regions other than the US one**: unchecked (see "Regions").

## What the game gives us

From reading `ifprize.c`, `gmapproach.c`, `gm_1BFA.c`, `gm_16F1.c`, `gm_1736.c` and `gmvsmelee.c`, and from driven runs on 2026-10-29.

### The unlock notice scene

- **One notice at a time** in a frame: a message (game text), the date and time the unlock was recorded ("10.29.2026  17:50:01"), a category icon (a D-pad for a mode, sliders for a rule, a note for Sound Test, 全 for all characters or stages, a trophy), and one of three jingles.
- **The queue** is built before the scene starts (`gm_ModeState_Prize_OnEnter`, `gm_1BFA.c`): every unlock not yet shown, in ascending order of its unlock number, then each newly got trophy, then any unlocks those caused. There is no limit. Each notice shown is marked seen in the save, so it is shown once.
- **Input**: about 6 frames after entry the first notice appears. A or Start on any controller shows the next one; there is no minimum display time. After the last, a short closing animation, then the scene ends.
- **Every agent run passes through 8 notices** after the title screen's Start: with no memory card the base port builds a save with everything unlocked and nothing seen (`gmmain_lib.c`). In order: "1-P All-Star mode is now open! Better practice up!", "Random Stage Select is now available in Additional Rules!", "Check out smashing sounds! Find Sound Test under Data.", "You've unlocked all playable characters! Get to fighting!", "All stages are now open!", then three trophies ("You got the Kirby Hat 4 trophy!", "…Kirby Hat 5…", "…Marin…"). `tools/a11y/main_menu.drive` presses A through them.

### Its words

- The messages are game text in `SdPrize.usd` (`SIS_PrizeData`), loaded into font slot 2. 393 strings.
- `un_802FE918(kind, trophy_id, timestamp)` sets up each notice, the first and each next. It maps the unlock number through `un_803F9B30` to a string number (unlock number + 2 for numbers 0 to 0x3D) and binds it to the frame's text (`un_803F9D48.x6`).
- **A trophy notice** (unlock number 0x3E) is composed at run time into the placeholder string 0x4A: string 65 ("You got / the "), the trophy's name (string id + 79 on a US setup; `Toy_803063D4`), then string 66 (" trophy!"), with `HSD_SisLib_803A6530` and `HSD_SisLib_803A660C`. Reading string 0x4A after the composition reads the whole message.
- **"Pokémon"** uses glyph 0x4000, a glyph from the screen's own font file, which `decode_game_text` does not know yet; "Mushroom Kingdom ll" is spelled with two lowercase Ls on the disc; several strings carry double or trailing spaces.
- The unlock numbers run 0 to 0x41: the 11 characters, 11 stages, All-Star, Score Display, Random Stage, Mew and Celebi seen, Sound Test, all characters, all stages, mode clears, event counts, Target Test, 100-Man and 15-Minute Melee, Very Hard clears, VS match counts, trophy counts, a trophy, two special trophies and Event 51.

### The New challenger screen

- **What it shows**: "A new foe has appeared!", a black silhouette of the challenger in a panel, and "WARNING / CHALLENGER APPROACHING" with an exclamation icon. Pictures only (the `NtAppro` archive); the name appears nowhere. The approach music plays.
- **Its state**: `gm_Scene_Approach_OnEnter` gets the challenger's `CharacterKind` and the challenged human's port, from the challenger data `gm_InitChallengerData` filled (`gm_1736.c`).
- **Input**: after 180 frames (3 seconds), A or Start on the challenged human's controller only (`gm_Scene_Approach_OnFrame`).
- **Who is challenged**: in VS, one human from the match's standings (`findSmallestLoser`, `gmVsMelee_ExitResults`); in single-player modes, the one player.
- **What follows**: a one-stock fight on the challenger's stage. Won: the character is unlocked and the unlock notice scene follows ("You've unlocked the singing wonder, Jigglypuff!"). Lost: back to the previous mode, with no results screen.
- **When it comes**: only while a character is locked. In VS, `gmVsMelee_ExitResults` checks after every match with a human, a match quit from the pause included, whether the save's VS match count has reached a locked character's threshold (Jigglypuff 50, Dr. Mario 100, Pichu 200, Falco 300, Marth 400, Young Link 500, Ganondorf 600, Mewtwo 700, Luigi 800, Roy 900, Mr. Game & Watch 1000; `gm_80172DD4`), then Mewtwo by VS play time and Marth by starters used.
- `scene_speech.cpp` says "Unlock notice. No speech yet." and "New challenger" for the two scenes today, from `pc_a11y_scene_entered`, which fires after the scene's enter function.

### The save file

From `src/sysdolphin/baselib/card.c` and `crypt.c`, `src/melee/lb/lbcardgame.c` and `gm/types.h`, confirmed by a working prototype (`prototype/` beside this spec):

- The GCI is a 0x40-byte directory entry, then 11 sectors of 0x2000 bytes. Sector 0 holds the comment, banner and icons: the game's own pictures, so **no save file is ever committed**.
- Each data sector is one block: a 0x20-byte header (a 16-byte checksum, the block id, a sequence number, a file table), then the payload. `HSD_Encrypt` checksums and scrambles each block; `HSD_Decrypt` rejects a block whose checksum fails. The prototype's port re-encrypts a fresh save byte for byte.
- Block id 1 holds `GmSaveData` (0x1790 bytes), in two copies (sectors 1 and 10 of a fresh save), little-endian on PC. Offset 0x00 is the unlocked characters' bits, 0x08 the VS match count, 0x0C the VS play time.
- A save with every character but one unlocked and the VS match count at that one's threshold brings the challenger after every VS match, run after run, until it is beaten; the game keeps the edited count.

## Decisions

### Hosting

- **Unlock notices**: one hook, `pc_a11y_unlock_notice(text_id)`, at the end of `un_802FE918` (`ifprize.c`), passing the string number bound to the frame (0x4A for a trophy, after its composition). Declared in `a11y_hooks.h`, gated in `hooks.cpp` by `game_hook_may_speak()`.
- **The words** are read by number from `SIS_PrizeData` on font slot 2 through `a11y_game_string` with a symbol check, decoded with `decode_game_text` and spoken with `spoken_text`, as the menu descriptions and results are. An undecodable text is logged once through `LogOnce`. Glyph 0x4000 decodes as "é" on this table; the disc's spelling ("Mushroom Kingdom ll") is kept, as the screen shows it.
- **New challenger**: said from `pc_a11y_scene_entered` for `GS_APPROACH`, with the challenger's character and the challenged port read from the challenger data by `game_access.c`. A hook in `gm_Scene_Approach_OnEnter` is the fallback if the data cannot be read cleanly. Whether the match had two or more humans comes from the same place or from the VS match that preceded it; the implementer finds the cleanest source.
- **Read only.** Neither reader writes anything in the game.

### What a notice says

- **Each notice**: its text, interrupting the one before. "You've unlocked all playable characters! Get to fighting!"
- **The first notice of a visit** adds the key: "1-P All-Star mode is now open! Better practice up! A for next."
- **Left out**: the date and time, the category icon, and a count ("1 of 8"), which the screen does not show.
- **Scene words**: `GS_PRIZE_INTERFACE` becomes silent in `scene_speech.cpp`; the first notice is the opening. Nothing is said as the scene closes; the next scene names itself.

### What the New challenger screen says

- **On entry**, replacing "New challenger": the screen's words, the challenger's name for the silhouette, and the key. "Warning! Challenger approaching! A new foe has appeared: Jigglypuff. A to fight."
- **With two or more humans** in the match that brought the challenger, the challenged player's label before the key: "… Jigglypuff. P2, A to fight." The player label is the match announcements' (`CONTEXT.md`): name tag where there is one, otherwise the port.
- The key is said at once, though A works only after 3 seconds; an early press does nothing.
- The character's name comes from the fork's existing character names (character select's string table).

### Regions

Only the US disc is checked. `pc_region_sis_index` has no PAL shift for `SIS_PrizeData`, and a Japanese setup loads `SdPrize.dat` instead. A wrong table or an undecodable text is logged once and the notice is said without it.

## The challenger save tool

- **`tools/a11y/make_save.py`** (issue 02): takes a fresh save and writes one where a named challenger comes after the next VS match. Built from the prototype, with room for other edits later (an unlock left pending, for instance).
- **The fresh save** it starts from is made by the game itself, the first time it is needed: a `drive.py --card --scene ""` run with an empty card answers Yes to "Create Game Data?" (`.scratch/memory-card/issues/01-drive-card.md`). It is kept under `build/`, never committed.
- **A drive script** loads the challenger save with `drive.py --card`, plays a VS match quit at once from the pause (about 23 seconds from launch to the screen), and checks the New challenger line.
- **For the maintainer**: one command that makes a test folder with the challenger save and a copy of their `a11y.ini`, and starts `build\melee.exe` there with `MELEE_USER_DIR` and `MELEE_CACHE_DIR` pointing at it, never touching `%APPDATA%\melee-pc`. Copying the save in again resets it. The game is the maintainer's to focus, so this launch may take focus, unlike the drivers.
- **Never with a boot scene**: a boot scene skips loading the card and the first save overwrites it (`.scratch/boot-scene/issues/01-boot-scene-overwrites-save.md`).

## Tests

- **Unit tests** (a new `unlock_speech` suite, or within `scene_speech`): a notice said, the key on the first of a visit only, the next interrupting, a trophy notice, an undecodable text; the New challenger line with and without two humans, an unknown character.
- **Decoder test** (`test_game_text`): glyph 0x4000 on the prize table.
- **Drive scripts**: `main_menu.drive`'s 8 notices, now said (it waits on the main menu, so it keeps working); a script that checks the 8 texts and the key on the first only; the challenger script.
- **Out of reach for scripts**: winning the challenger fight. Four scripted fights against the weakest CPU all lost, so the unlock notice after a win is left to the play test; the notice reader itself is checked by the 8 notices.

## Docs

- `docs/a11y/native-menus.md`: an "Unlock notices and challengers in detail" section with the scene, the queue, the words, the challenger's path and thresholds, and the save file's layout; the screen inventory pointing to it.
- The `verify` skill: the new scripts, their expected lines, and the challenger fight as out of reach.
- `CLAUDE.md`: one "Accessibility status" line for unlock notices and the New challenger screen, and the save tool in "Driving the game".
