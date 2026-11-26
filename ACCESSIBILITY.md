# Non-Visual Melee

Non-Visual Melee is an accessibility fork of
[melee-pc](https://github.com/999sian/melee-pc), the PC port of Super Smash
Bros. Melee. It aims to make the game playable by blind players. Menus are read
aloud through your screen reader. In a match, announcements in two voices and
audio cues tell you where you and your opponents are, warn of gaps and ledges,
keep you up to date on damage and stocks, and lead you to items. You can play
online over LAN and Direct Connect against both Non-Visual Melee and melee-pc
players on the same melee-pc version.

It doesn't claim parity with sighted players: the game's speed, the precision it
asks for and the complexity of its stages make that out of reach. Even so, I
think you can have a lot of fun with it. The current best expereince is against
a single opponent on a simple stage with items set to low or none. See it in
action on this [demo video](https://youtu.be/lR8rautqM7Q).

This is a beta. Things will change, not every part of the game is supported yet,
and some modes, such as Adventure and Target Test, may never be fully supported.
melee-pc itself is also a beta, so expect issues in the game itself, crashes
included. For questions, bug reports, feature ideas and perhaps finding people
to play with online, check out Non-Visual
Melee's [discord channel](https://discord.gg/muRgv2GYHe) on the Accessibility disco server.

## Features

### What is included

#### Playable modes

With the features below, these modes are accessible:

- **1P Mode:** Training Mode and Multi-Man Melee.
- **VS. Mode on one computer:** Melee and Special Melee.
- **VS. Mode online:** Direct Connect with a friend's connect code, and LAN
  play with players on the same network.

#### Menus and game screens

- **Port menus.** The launcher, which opens before the game, and the port menu,
  opened in game with F1 or Back on a controller, are read aloud. They hold
  melee-pc's own settings, such as the master, music and sound effect volumes
  and controller remapping.
- **Update check.** As it opens, the launcher looks for a newer release of
  Non-Visual Melee and, when there is one, says "Update available" with its
  version; Open release page takes you to it in your browser.
- **Accessibility settings.** An Accessibility category in both menus holds
  Non-Visual Melee's own settings: switches for speech and the audio cues, each
  cue's volume, and the voices of the match announcements with their rate and
  volume. Each cue and each voice has a Preview that plays a short sample, so
  you can learn the sounds before a match.
- **The game's menus.** The main menu and the menus under it, such as 1P Mode
  and VS. Mode, say each screen's name, the option you are on and the game's
  description of it.
- **Character select and stage select.** On these screens the game has you move
  a free cursor around the screen rather than step through a list. Here the
  D-pad steps the cursor from one thing on the screen to the next, saying where
  it goes, and moves it there for you.
- **Rules.** The Custom Rules screen, where you set up VS. matches (time or
  stock, the number of stocks, handicap and so on), and the screens under it,
  such as Item Switch.
- **Training menu.** The pause menu of Training Mode, where you set the game
  speed, items, the number of CPU opponents, what they do and their damage.
- **Results.** After a match, the results screen says the winner and each
  player by place with a short summary. The D-pad then lets you explore the
  game's detailed statistics: Up and Down read the rows of a page, and Left and
  Right switch between players, and Z says the summary of the player you are
  on.

#### Gameplay

##### Match announcements

During a match, two voices of their own, beside your screen reader, tell you
what the game's display shows: your voice speaks about you, and the opponents'
voice about everyone else, teammates included.

- **Damage.** Each player's damage once it settles, either on every change or
  only when it passes a set interval.
- **Stocks.** Each stock lost.
- **Items.** Each item a player takes, in that player's voice. Your screen
  reader names each item as it appears.
- **Time left.** In a timed match, your screen reader says the time left at
  each whole minute and at 30 seconds.
- **The voices.** Windows' own OneCore voices, the ones Narrator uses, Zira
  for you and Mark for the opponents by default.
- **The settings.** In the accessibility settings you can pick any OneCore
  voice installed on your computer, set each one's rate and volume, and
  choose what is announced.

##### Audio cues

During a match, sounds of their own tell you where things
are. Each can be switched off and its volume set in the accessibility
settings.

- **Your position.** A tick that pans left and right with where you are on
  the stage, rises in pitch the higher you are, and quickens as the nearest
  opponent gets closer.
- **Opponent's position.** A lower pulse that says where the nearest
  opponent is from you: panned to their side and higher in pitch when they
  are above you. Switched to the stage instead, it pans and pitches with
  where they are on the stage, as your own tick does.
- **Gap warning.** A low hum while there is no floor below you, such as
  when you are off the stage: fall from there and you are knocked out
  unless you recover.
- **Ledge warning.** A pulsing chord on the side of a nearby edge of the
  floor below you that has a lethal gap past it, louder and nearer the middle
  as you get closer to it. An edge with more floor below it, such as a
  platform's, gives no warning.
- **Item appearance cue.** A sound where each item appears, from you by
  default: panned to the side it is on and higher in pitch when it is
  above you. With Items relative to you off, it pans and pitches with
  where the item is on the stage instead. 
- **Item's position cue.** A pulse at a steady pace from the nearest item
  lying loose, placed as the item appearance cue is, from you or on the
  stage. It quickens while the item is within reach.

##### Positional sound effects

The game itself plays every fighter's sounds in the middle. Non-Visual
Melee places them instead: an opponent's attacks, jumps, landings and the
hits they take come from their side of you, and grow quieter the further
away they are, so you can hear where the action is. Your own fighter's
sounds stay in the middle at full volume. In the accessibility settings,
under Cue settings, you can switch it off, choose whether sounds are placed
from you or across the stage, and set how quiet far sounds get.

For this to work, the game's own sound must be in stereo: in the game's
Options, Sound, choose Stereo rather than Mono.

##### Stage sounds

Some stages make sounds of their own: Corneria's Arwings and Great Fox,
Mute City's cars, Pokémon Stadium's crowd. Switch Stage sounds off, in the
accessibility settings under Cue settings, to mute them so the fighters are
easier to hear. A stage hazard then comes without its sound, so keep it on
if you rely on hearing one coming.

The echo some stages add to every sound, such as on Final Destination, is
a different setting: Reverb, with the volumes in the launcher's Settings
and in the port menu. Switch it off for dry, easier to place sounds.

### What is not included

These parts of the game are not accessible yet:

- **1P Mode:**
  - **Classic.** You can choose your character and set the difficulty, and
    the matches themselves have the cues and announcements, but the screens
    between them, such as the match intros, the results and the continue
    screen, are not read.
  - **Adventure.**
  - **Event Match.** Its menu of events is not read.
  - **Stadium:** Home-Run Contest and Target Test. Multi-Man Melee is
    accessible.
- **VS. Mode:** Tournament.
- **Online:** Ranked and Unranked matchmaking, and the Profile screen.
- **Trophies:** everything under the Trophies menu.
- **Data:** everything under the Data menu.

## Requirements

- **Windows 10 or 11**, 64-bit. melee-pc runs on other systems too, but
  Non-Visual Melee supports only Windows for now.
- **Your own copy of the game** as a disc image: Super Smash Bros. Melee, USA
  revision 2 (NTSC-U 1.02, game id GALE01), as an `.iso`, `.gcm`, `.ciso` or
  `.rvz` file. No game data comes with Non-Visual Melee. The launcher can
  check the image against a known good copy. melee-pc also
  boots the European (PAL) disc, but Non-Visual Melee has not been tested with
  it.
- **A screen reader**, such as NVDA or JAWS, for the menus. Without one, a
  Windows voice speaks instead.
- **Windows OneCore voices** for the match announcements. Windows comes with
  some; more can be added in Windows' speech settings.
- **A graphics card that supports Direct3D 12 or Direct3D 11.** Any Intel graphics from 2014
  (Broadwell) on, AMD GCN or newer, or NVIDIA Fermi or newer will do. The CPU
  needs are light.
- **A controller or a keyboard.** Any gamepad SDL3 knows (Xbox, PlayStation, Switch Pro, …), and an
  official GameCube controller adapter is read directly.

## Getting started

1. Download the latest release from the
   [releases page](https://github.com/ohylli/non-visual-melee/releases/latest).
2. Extract the downloaded zip file into a folder of your choice. Beside
   `melee.exe` the folder holds `readme.html`, this guide, and
   `changelog.html`, what changed in each release.
3. Run `melee.exe` from that folder. It opens the launcher, and on the first
   start it asks you to choose your disc image with Choose Disc.
4. If you are unsure you have the right disc image, press Verify Disc to check
   it.
5. Press Play to start the game.

### First steps in the game

- **Learn the sounds.** In the Accessibility category of the launcher's
  Settings, or of the port menu (F1 in game), open Cue settings and play each
  cue's Preview to hear what it sounds like. Under Match announcement settings,
  Voice settings lets you hear and choose the two voices and set their rate and
  volume.
- **Practise in Training Mode.** In 1P Mode, choose Training Mode and a simple
  stage. Pokémon Stadium, open from the start, is the easiest place to get
  used to the cues: a wide, flat main floor with two low platforms above it
  and no hazards. In other modes it changes shape now and then, but never in
  Training Mode; in a VS. match, Hazardless stages, in the Gameplay category
  of the launcher's Settings or of the port menu, keeps it as it is. Yoshi's
  Story, also open from the start, is smaller and never changes: a good next
  step for learning the ledges. On both stages you start on one of the
  platforms above the main floor; tap down on the control stick to drop
  through it. Final Destination, one flat floor with no platforms at all, is
  locked at first; Unlock everything, in the Gameplay category, unlocks it
  along with all the characters and other stages. The Training menu's Item
  row brings an item in beside you, a good way to learn the item cues.
- **Play a match.** When you are ready, choose Melee in VS. Mode and set up a
  match against a CPU opponent.

## Controls

The default controls of the game. Other gamepads follow the Xbox layout by
position: the bottom face button is A, the right one B, and so on. You can
change the controller's buttons in the Controls category of the port menu.

| Action | Keyboard | Xbox controller | GameCube controller |
|---|---|---|---|
| Move | Arrow keys or W, A, S, D | Left stick | Control stick |
| Attack | X | A | A |
| Special attack | Z | B | B |
| Jump | C or V | X or Y | X or Y |
| Shield, or dodge with a direction | Q or E | Left or right trigger | L or R |
| Grab | Tab | Right bumper | Z |
| Smash attack | I, J, K, L | Right stick | C-stick |
| D-pad | T, F, G, H for up, left, down, right | D-pad | D-pad |
| Pause | Enter | Menu button | Start |
| Port menu | F1 | View button | None |

## Character select

Character select is where each player chooses a character before a match. It
is not a list: the game has each player move a hand, a pointer like a mouse
cursor, around one screen with the stick. Non-Visual Melee lets you step the
hand from one thing on the screen to the next with the D-pad instead (T, F, G, H
on keyboard). The stage select works in the same way.

### The layout

The screen has three bands, from top to bottom.

- **The top bar.** From left to right: Teams, which switches between a free
  for all and a team match (VS. modes only), the rules header, which says the
  current rules, such as "2-minute KO fest!", and opens Custom Rules, and Back,
  which returns to the menu.
- **The portraits.** A picture of each character, in two rows to start with
  expanding to three after you unlokc new characters. 
- **The player slots.** Four slots along the bottom, players 1 to 4 from left
  to right. Each has a button that switches the slot between human, CPU and
  closed, and in a team match a team button. A CPU's slot also has a slider for
  its level, from 1 to 9, and with the handicap rule on each slot has a
  handicap slider.

In 1P modes that have them, arrows beside your slot set the difficulty and
the number of stocks.

### Moving the hand

- **Left and Right** on the D-pad step to the next thing in the same band or
  row. **Up and Down** step to the row above or below, to whatever is nearest
  the hand. At an edge the hand stays where it is.
- Each press says where the hand is going, and the hand then glides there by
  itself. You can press again before it arrives.
- The hand only moves. You press the buttons yourself: A on a button presses
  it, and A on a slider grabs its knob, Left and Right move it one level, and A
  lets it go.

### Choosing a character

Your hand starts over your own player slot. Move it up into the portraits and
your coin, a token with your player number, comes into the hand, and each
character the coin passes over is named. Press A to put the coin down and choose that
character, and the game's announcer says the name. X and Y change the
character's costume, its colours.

To choose again, step to your coin, which is named with its character, and
press A to pick it up, or press B to call it back into your hand. B while
carrying a coin puts it back where it was.

### Starting a match against a CPU

1. Step down to player 2's slot button and press A until it says CPU. The CPU
   gets a character of its own; to choose another, pick up its coin from its
   portrait and put it down on another.
2. Set the CPU's level with its slider.
3. Choose your own character.
4. Once every slot has a character, press Start. Stage select comes next.

## Online play

You can play VS. matches online against other Non-Visual Melee players and
against sighted players using melee-pc, as long as you both have the same
melee-pc version and the same disc image. Both are found under VS. Mode,
Online.

Non-Visual Melee numbers its releases apart from melee-pc. Each release names
the melee-pc version it plays with, at the top of its release notes: a melee-pc
player needs that version, and another Non-Visual Melee player a release that
names the same one.

### How it works

There is no game server. Your computer connects straight to your friend's
computer (peer to peer), and both computers run the match side by side,
sending each other only the controller presses. When a press arrives late,
the game quietly corrects itself, so the match stays the same on both sides.
This is called rollback. A lower ping between you two gives a smoother match,
so playing someone nearby works best.

The match uses the rules of player 1, the player who started the session, items
included.

### LAN play

LAN play finds other players on the same home network by itself. Choose LAN
play and the game lists the computers it finds and says when one appears or
leaves. When at least one other player is found, press Start to begin; the one
who presses Start becomes player 1, and the other follows.

### Direct Connect

Direct Connect plays with a friend over the internet using connect codes.

- **Your connect code.** Every player has one: a name followed by # and eight
  letters and digits. The eight characters stay the same, so you can share
  your code once and reuse it. On the Direct Connect screen, Y copies your
  code so you can paste it into a chat message to your friend. The Online
  category of the launcher's Settings also shows it.
- **Your name.** The name part is your Player name, up to eight letters or
  digits, set in the Online category of the launcher's Settings or the port
  menu. It only shows who you are; changing it keeps your eight characters,
  and a friend calling you types only those.
- **Calling.** One of you chooses Enter a friend's code and types the other's
  code, or chooses Call from clipboard when the code has been copied. The other
  either chooses Wait for a friend to call me or calls back the same way. Both
  work: two players who call each other at the same time also find each other.
- **Recent opponents.** People you have played before stay in the list, so
  calling them again is one press.
- **Waiting.** Finding each other usually takes some seconds, sometimes longer.
  While the game waits, it repeats what it is doing from time to time, so you
  know it has not stopped.

### Limitations

Because there is no server in between, the two computers must be able to reach
each other through their routers. Most home networks allow this, but not every
network does.

- **Some networks can't be reached.** Some mobile and phone connections, and
  many workplace, school and dormitory networks, hide computers in a way that
  stops a direct connection. A VPN can also get in the way. If one of you is on
  such a network, a connection may still work when the other's network is
  more open; if both are, it won't.
- **No relay fallback.** If a direct connection fails, there is no server to
  route the match through instead.
- **Two players at the same home.** Direct Connect usually fails between two
  computers behind the same router. Use LAN play instead.
- **Ranked and Unranked.** The matchmaking modes, which find you a stranger to
  play, are not read aloud yet.

### Checking your connection

`melee-netcheck.exe`, in the same folder as `melee.exe`, checks whether your
network suits Direct Connect. It is a plain console program, read by your
screen reader, and the check takes up to a quarter of a minute.

1. Run `melee-netcheck.exe`. If Windows asks whether to allow it on networks,
   choose Allow.
2. It asks servers on the internet how they see your computer and ends with a
   result: good, likely trouble, blocked, not sure or no internet, each with a
   short explanation.
3. Then, if you like, you can test with a friend: you both run the program, and
   each types the other's address. The program tells you your own address and
   copies it to the clipboard, so you can send it to your friend; keep the
   window open meanwhile, as the address holds only while it runs. It reports
   whether you reached each other both ways, one way only or not at all, and
   how long a round trip took.

The friend test is optional. If both of you get a good result, just go ahead
and try Direct Connect in the game. The test helps when a call fails or a
result was not good, to find which side has the problem. Everything the
program says is also saved in `melee-netcheck-result.txt` beside it, so your
friend can send you their results.
