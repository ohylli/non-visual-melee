# Non-Visual Melee

The accessibility fork of melee-pc: work to make the PC port of Super Smash Bros. Melee playable by blind players through screen reader speech and audio cues. This glossary covers the fork's own language, plus the few terms about the code it builds on that are easy to confuse.

## Language

### Where code comes from

**Base port**:
The original melee-pc project that this fork is derived from and keeps tracking.
_Avoid_: Upstream (relative: this fork is upstream to anyone who forks it), original, parent

**Decomp layer**:
The decompiled code of the original game, which the base port imports from the community Melee decompilation project.
_Avoid_: Game code, engine, the decomp

**Port layer**:
The code that makes the decomp layer run on a PC: windowing, graphics, audio, input, the launcher and the port menu. The fork's own code lives inside this layer.
_Avoid_: Platform code, PC code

**Decomp sync**:
A change made by the base port that refreshes the decomp layer from the decompilation project.
_Avoid_: Decomp update, decomp merge

**Base merge**:
The fork bringing the base port's latest work into its own history.
_Avoid_: Upstream merge, sync, rebase

### How the fork attaches

**Hook**:
A single-line call from a file owned by the base port into the fork's code, placed at the moment something meaningful to the player happens.
_Avoid_: Patch, injection, event, callback

**String table**:
A fork-owned mapping from a game identifier (a menu entry, a character, a stage) to the English words spoken for it. Needed because the game draws most labels as images rather than text.
_Avoid_: Translations, labels, text map

### What the player hears

**Announcement**:
One complete unit of speech sent to the screen reader: everything the player needs to hear about the thing that just changed, composed as a single text. A setting and its value are one announcement, not two. An announcement either interrupts current speech or is queued after it; queueing is for separate happenings, never for the parts of one.
_Avoid_: Message, utterance, speech line, TTS string

**Cue**:
A non-speech sound the fork plays to convey game state during play, as distinct from the game's own sound effects.
_Avoid_: Beep, sound effect, SFX, earcon

**Speech log**:
The developer-facing record of every announcement and cue, written as it happens.
_Avoid_: TTS log, debug output

### How speech is produced

**Speech**:
The fork's subsystem that takes announcements from every feature, writes the speech log and honours the accessibility switch. The only part of the fork that knows a screen reader exists.
_Avoid_: TTS, narrator, announcer (the game has its own announcer voice: "Ready, go!")

**Screen reader bridge**:
The thin layer inside speech that talks to the screen reader library and holds every platform-specific detail. Nothing else in the fork touches the library.
_Avoid_: Prism wrapper, TTS backend, speech backend

### The three user interfaces

Never say plain "menu"; always say which of these is meant.

**Native menu**:
A menu belonging to the original game, such as the main menu, character select or stage select.
_Avoid_: Game menu, in-game menu, original menu

**Port menu**:
The settings overlay added by the base port, opened with F1 while the game runs.
_Avoid_: Settings menu, overlay, F1 menu, pause menu

**Launcher**:
The window the port shows before the game starts: choosing and checking the disc image, settings, and update notices.
_Avoid_: Startup menu, front end

**Setting**:
One named option of the port that the player can change, with a current value and a line of help text. The same setting can appear in both the launcher and the port menu. What the game itself lets the player change on a leaf screen is a row and its value.
_Avoid_: Option, preference, config entry

### Native menus

**Scene**:
One full-screen step of the game's flow, such as the title screen, the main menu tree, character select or a match.
_Avoid_: Screen (a scene can hold many), stage (the game has stages to fight on), state, mode

**Main menu tree**:
All the native menu screens reached from the main menu without starting a mode, from "1-P Mode" down to "Sound".
_Avoid_: Main menu (that is only its top screen), menu system

**Menu screen**:
One screen of the main menu tree.
_Avoid_: Submenu, page, menu

**Tree screen**:
A menu screen that is a list of entries, each leading to another menu screen or starting a mode.
_Avoid_: Tree menu, list menu, submenu

**Leaf screen**:
A menu screen at the end of the tree, where the player sets or views something.
_Avoid_: Settings screen, option screen, sub-screen

**Entry**:
One line of a tree screen that the player can choose.
_Avoid_: Item (the game has items in matches), option, row, button

**Hovered entry**:
The entry the cursor is on, as distinct from one the player has confirmed.
_Avoid_: Selected entry, focused entry, current entry

**Row**:
One line of a leaf screen, holding a value the player can change.
_Avoid_: Setting (that is the port's), option, entry (that belongs to a tree screen)

**Value**:
What a row is currently set to.
_Avoid_: State, choice, setting

**Description**:
The game's own one-line text about the hovered entry or the current row, shown at the bottom of a menu screen.
_Avoid_: Help text (that belongs to a setting), tooltip, hint

**Picture label**:
A name or value the game draws as an image, so there is no text to read back and the words come from a string table.
_Avoid_: Texture label, image text

**Game text**:
Text the game draws with its own text system, which the fork can read back as words.
_Avoid_: SIS text, in-game text, strings

**Free-cursor screen**:
A native menu where a cursor moves freely over a layout and what it points at is decided by position, such as character select and stage select.
_Avoid_: Free cursor menu, pointer screen

### Wording

Write "accessibility" in prose. `a11y` appears only in identifiers and paths.
