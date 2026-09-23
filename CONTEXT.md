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
The window the port shows before the game starts, where the disc image is chosen and checked.
_Avoid_: Startup menu, front end

### Wording

Write "accessibility" in prose. `a11y` appears only in identifiers and paths.
