# Non-Visual Melee

Non-Visual Melee is an accessibility fork of
[melee-pc](https://github.com/999sian/melee-pc), the PC port of Super Smash
Bros. Melee. It aims to make the game playable by blind players. Menus are read
aloud through your screen reader. In a match, announcements in two voices and
audio cues tell you where you and your opponents are, warn of gaps and ledges,
and keep you up to date on damage and stocks. You can play online over LAN and
Direct Connect against both Non-Visual Melee and melee-pc players on the same
version.

It doesn't claim parity with sighted players: the game's speed, the precision it
asks for and the complexity of its stages make that out of reach. Even so, I
think you can have a lot of fun with it.

This is a beta. Things will change, not every part of the game is supported yet,
and some modes, such as Adventure and Target Test, may never be fully supported.
melee-pc itself is also a beta, so expect issues in the game itself, crashes
included.

## Features

### What is included

- **Playable modes.** With the features below, these modes are accessible:
  - **1P Mode:** Training Mode and Multi-Man Melee.
  - **VS. Mode on one computer:** Melee and Special Melee.
  - **VS. Mode online:** Direct Connect with a friend's connect code, and LAN
    play with players on the same network.
- **Port menus.** The launcher, which opens before the game, and the port menu,
  opened in game with F1 or Back on a controller, are read aloud. They hold
  melee-pc's own settings, such as the master, music and sound effect volumes
  and controller remapping.
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
- **Match announcements.** During a match, two voices of their own, beside your
  screen reader, tell you what the game's display shows: your voice speaks
  about you, and the opponents' voice about everyone else, teammates included.
  They say each player's damage once it settles, either on every change or only
  when it passes a set interval, and each stock lost. Your screen reader says
  the time left in a timed match, at each whole minute and at 30 seconds. The
  voices are Windows' own OneCore voices, the ones Narrator uses, Zira for you
  and Mark for the opponents by default. In the accessibility settings you can
  pick any OneCore voice installed on your computer, set each one's rate and
  volume, and choose what is announced. 
- **Audio cues.** During a match, sounds of their own tell you where things
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
- **A graphics card that supports Direct3D 12 or Direct3D 11.** . Any Intel graphics from 2014
  (Broadwell) on, AMD GCN or newer, or NVIDIA Fermi or newer will do. The CPU
  needs are light.
- **A controller or a keyboard.** Any gamepad SDL3 knows (Xbox, PlayStation, Switch Pro, …), and an
  official GameCube controller adapter is read directly. 

## Getting started

1. Download the latest release from the
   [releases page](https://github.com/ohylli/non-visual-melee/releases/latest).
2. Extract the downloaded zip file into a folder of your choice.
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
  stage such as Final Destination: one flat floor with no platforms is the
  easiest place to get used to the cues. Final Destination is locked at first;
  Unlock everything, in the Gameplay category of the launcher's Settings,
  unlocks it along with all the characters and other stages.
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
hand from one thing on the screen to the next with the D-pad instead.

### The layout

The screen has three bands, from top to bottom.

- **The top bar.** From left to right: Teams, which switches between a free
  for all and a team match (VS. modes only), the rules header, which says the
  current rules, such as "2-minute KO fest!", and opens Custom Rules, and Back,
  which returns to the menu.
- **The portraits.** A picture of each character, in three rows. With every
  character unlocked:
  - Top row: Dr. Mario, Mario, Luigi, Bowser, Peach, Yoshi, DK, C. Falcon,
    Ganondorf.
  - Middle row: Falco, Fox, Ness, Ice Climbers, Kirby, Samus, Zelda, Link,
    Young Link.
  - Bottom row: Pichu, Pikachu, Jigglypuff, Mewtwo, Mr. Game & Watch, Marth,
    Roy.

  A character not yet unlocked is left out, and the D-pad skips it. Sheik is
  played by choosing Zelda.
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

This readme is a work in progress; its content is being planned.
