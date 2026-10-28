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
One complete unit of speech, sent to the screen reader or spoken by a voice: everything the player needs to hear about the thing that just changed, composed as a single text. A setting and its value are one announcement, not two. An announcement either interrupts current speech or is queued after it; queueing is for separate happenings, never for the parts of one.
_Avoid_: Message, utterance, speech line, TTS string

**Cue**:
A non-speech sound the fork plays to convey game state during play, as distinct from the sound effects the game plays by itself. A cue is usually the fork's own sound, but can be one of the game's sounds that the fork chooses to play.
_Avoid_: Beep, sound effect, SFX, earcon

**Cue engine**:
The fork's own sound player for cues, with its own output beside the game's audio. Cues that are game sounds go through the game's audio instead.
_Avoid_: Mixer, audio backend, sound system

**Cue volume**:
The loudness of all cues together, set under the port's master volume, which the cues follow like every other sound.
_Avoid_: Master volume (that is the port's), cue master

**Position cue**:
A pulsed cue whose pan and pitch say where a fighter is on the stage: pan across it, pitch how high. There are two: **your position cue**, whose pulse also quickens as the opponent comes near, and **the opponent's position cue**, a lower pulse at a steady pace.
_Avoid_: Locator, radar, beacon, player cue

**Gap warning**:
A held, low cue that sounds while you are in the air with no floor anywhere below you, platforms included, so that falling straight down would take you past the bottom blast zone. It says nothing of whether you can still get back; the game's own sound near the bottom blast zone is a separate warning.
_Avoid_: Fall cue, death warning, void cue, offstage cue

**Ledge warning**:
A pulsed cue on the left or right that sounds while you are near an end of the floor below you past which nothing lies, so that walking or being knocked off it would take you past the bottom blast zone. It pans from the side towards the centre and grows louder as the end comes near, on the ground and in the air. A platform's end over the main floor is no ledge here; past the end the gap warning takes over.
_Avoid_: Edge cue, cliff warning, ledge cue (a ledge you can grab is the game's own idea, not this one)

**Speech log**:
The developer-facing record of every announcement and cue, written as it happens.
_Avoid_: TTS log, debug output

### How speech is produced

**Speech**:
The fork's subsystem that takes announcements from every feature, writes the speech log, says each through the screen reader or a voice, and goes quiet while the Speech or Accessibility switch is off. The only part of the fork that knows a screen reader or a voice exists.
_Avoid_: TTS, narrator, announcer (the game has its own announcer voice: "Ready, go!")

**Voice**:
A Windows voice the fork speaks with itself, beside the screen reader, so the player hears at once whom an announcement is about. There are two: **your voice**, for what happens to you, and **the opponents' voice**, for what happens to everyone else in the match, teammates included. Everything else stays on the screen reader.
_Avoid_: TTS voice, channel, speaker, narrator, player voice

**Match announcement**:
An announcement about a match as it is played: a player's damage, their stocks, the time left.
_Avoid_: Callout, combat speech, match speech

**Player label**:
The words a match announcement names a player by when the voice alone does not tell whom it is about: the player's name tag where they have one, otherwise their port ("P2") or, for a computer player, "CPU" and their slot ("CPU 3"); a teammate's label starts with "teammate". The character's name is never used.
_Avoid_: Player name, tag, port name

**Settled damage**:
A player's damage, as the HUD shows it, once it has held still for a moment: what a damage announcement says, so a flurry of hits is one line, not one per hit.
_Avoid_: Final damage, current damage, percent

**Damage threshold**:
A multiple of the damage interval the player chooses (20%, 40%, 60% at an interval of 20). In threshold mode a player's settled damage is said only once it has crossed one upward since it was last said.
_Avoid_: Milestone, step, breakpoint

**Screen reader bridge**:
The thin layer inside speech that talks to the screen reader library, for the screen reader and the voices alike, and holds every platform-specific detail. Nothing else in the fork touches the library.
_Avoid_: Prism wrapper, TTS backend, speech backend

### The three user interfaces

Never say plain "menu"; always say which of these is meant.

**Native menu**:
A menu belonging to the original game, such as the main menu, character select or stage select.
_Avoid_: Game menu, in-game menu, original menu

**Port menu**:
The settings overlay added by the base port, opened with F1 or a controller's Back button while the game runs. The game stands still until it closes.
_Avoid_: Settings menu, overlay, F1 menu, pause menu

**Launcher**:
The window the port shows before the game starts: choosing and checking the disc image, settings, and update notices.
_Avoid_: Startup menu, front end

**Setting**:
One named option the player can change in the launcher or the port menu, with a current value and a line of help text, whether the base port's or the fork's. The same setting can appear in both. What the game itself lets the player change on a leaf screen is a row and its value.
_Avoid_: Option, preference, config entry

**Accessibility settings**:
The fork's own model of named values: the switches for accessibility, speech, steering and the cues, the volumes, and the tuning behind them, kept in a file of the fork's. Each value has a key, a default, a range, a unit and a one-line description; some are shown as settings, the rest are tuning values.
_Avoid_: Cue settings, a11y config, options, the ini (that is only the model's file)

**Tuning value**:
A value of the accessibility settings that no menu shows, changed only by editing the file; meant for developers setting the cues by ear.
_Avoid_: Hidden setting, developer setting, advanced option

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

**Rules screens**:
Custom Rules and the three leaf screens under it (Item Switch, Additional Rules, Random Stage), the same screens whether opened from the main menu tree or from character select's rules header.
_Avoid_: Rules menu, rules screen (when the whole group is meant), options

**Training menu**:
The native menu Training shows while the match is paused: a column of rows, most holding a value (Speed, Item, the CPUs), the rest an action that A carries out (spawning the chosen item, Reset, Finish).
_Avoid_: Pause menu (Training's pause is this menu; a VS pause has none), training pause screen

**Entry**:
One line of a tree screen that the player can choose.
_Avoid_: Item (the game has items in matches), option, row, button

**Hovered entry**:
The entry the cursor is on, as distinct from one the player has confirmed.
_Avoid_: Selected entry, focused entry, current entry

**Row**:
One line of a leaf screen or of the Training menu, holding a value the player can change, a way into another leaf screen (Custom Rules' "Item Switch"), or an action (the Training menu's "Reset").
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

**Target**:
Anything on a free-cursor screen that reacts to the cursor: a portrait, a button, a slider.
_Avoid_: Hotspot, item, element

**Steering**:
The fork moving the cursor of a free-cursor screen (a hand or the stage cursor) by generating stick input, as opposed to changing the cursor's position in game memory.
_Avoid_: Auto-move, snapping, warping

**Step**:
One D-pad press under steering, moving the cursor to the neighbouring target.
_Avoid_: Jump, hop, move

**Glide**:
The cursor's movement to a target under steering.
_Avoid_: Travel, animation, path

### Character select

**Hand**:
A player's cursor on character select, a white glove carrying the player's badge ("P1").
_Avoid_: Cursor (the decomp's word), pointer, glove

**Coin**:
The token a hand carries onto a portrait to choose a character.
_Avoid_: Token, chip, puck

**Portrait**:
One character's picture on character select, with the character's name drawn under it.
_Avoid_: Icon (the decomp's word), character slot, cell

**Rules header**:
The line across the top of character select that states the current rules ("2-minute KO fest!") and opens the rules screens.
_Avoid_: Rules button, rules banner, header

**Player slot**:
One of the four panels along the bottom of character select, with its HMN/CPU tab and name box.
_Avoid_: Door (the decomp's word), port, panel

### Stage select

**Stage cursor**:
The round crosshair that moves over stage select. Depending on the mode and rules it answers to every controller, to one player's, or online to each player's own.
_Avoid_: Hand (that is character select's), reticle, pointer

**Stage tile**:
One stage's picture on stage select, which the player picks to fight there.
_Avoid_: Icon (the decomp's word), cell, portrait

**Random tile**:
The tile labelled "Random", which picks a stage at random.
_Avoid_: Random button, random cell

**Locked tile**:
A stage tile drawn as a "?" because its stage is not yet unlocked. It can be hovered but not picked.
_Avoid_: Question mark, hidden stage (some locked stages are not drawn at all)

### Results

**Results**:
The scene after a VS, Special Melee, tournament or online match, where the announcer names the winner and each player looks over the match before the next one. Classic, Adventure and All-Star end on a different scene.
_Avoid_: Results screen, score screen, victory screen, end screen

**Summary table**:
The first view of results: the match type, the winner, each player's place, and a table of KOs, falls, SDs and one last row that depends on the match type (when each player was KO'd, a total, coins).
_Avoid_: Scoreboard, overview, standings

**Stats panel**:
One player's box of statistics, opened for every player by the first press on results and then paged and scrolled by that player alone.
_Avoid_: Stats screen, player card, panel (alone)

**Page**:
One of a stats panel's lists: KOs and falls against each opponent, the general statistics, or the bonuses.
_Avoid_: Tab, screen, view

**Bonus**:
A named award with points that results lists for a player, such as "Switzerland".
_Avoid_: Title, achievement, award

**Ready**:
A player's mark that they are done with results; the scene ends once every human player is ready, and a second Start takes it back.
_Avoid_: Confirmed, done, finished

**Reading cursor**:
The fork's own place in a stats panel's page, which the D-pad moves row by row and from player to player while the game's panels keep their own pages and scroll.
_Avoid_: Virtual cursor, review cursor, focus

### Online play

**Lobby**:
The native screen where players find each other before an online session starts, and where both return when it ends. LAN play, direct connect and internet play each have their own layout of it.
_Avoid_: Online menu, matchmaking, waiting room

**Connect code**:
A player's identity for Direct Connect, written as a name, a "#" and eight characters ("RYAN#K3X-Q2M-7A"). Only the eight characters identify the player; the name is a label they choose and can change.
_Avoid_: Friend code, player ID, address

**Direct Connect hub**:
Direct Connect's first screen: a list of whom to play (a code found on the clipboard, the recent opponents), a way into the code keyboard, and waiting for a friend to call.
_Avoid_: Hub (alone), Direct Connect menu, contact list

**Recent opponent**:
Someone this player has finished connecting to over Direct Connect, listed on the Direct Connect hub with how long ago they played.
_Avoid_: Contact (the base port's code word), friend, history

**Code keyboard**:
The grid of letters and digits on which a player types the eight characters of a friend's connect code.
_Avoid_: On-screen keyboard, keypad, code wheel

**Call**:
Trying to reach a friend by their connect code. The other way to meet is **waiting** for a friend to call; both players may call each other at once, and they still meet.
_Avoid_: Dial, connect (that is the session starting), invite

**Quick chat**:
The base port's set of fixed phrases a player sends to the other player online with two D-pad presses.
_Avoid_: Chat messages, emotes

### Wording

Write "accessibility" in prose. `a11y` appears only in identifiers and paths.
