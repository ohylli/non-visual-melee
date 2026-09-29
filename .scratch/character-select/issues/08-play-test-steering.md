# 08 Play-test steering by ear

Status: ready-for-human
Type: task
Blocked by: 05, 07

Maintainer, with NVDA and the controller or keyboard you normally use. Report what you hear and how it feels for each step; the agent then reads the `[a11y]` lines of `melee-pc.log` against the report. Expected behaviour is in `.scratch/character-select/spec.md`, the three "Stage 2" sections.

On the keyboard the D-pad is T (up), G (down), F (left) and H (right).

Anything unexpected becomes a new issue in this directory.

## A VS match against a CPU

1. Main menu, VS. Mode, Melee. After the opening announcement press Up. Expected: a portrait's name at once, and the game's move sound as the coin arrives.
2. Step along the row with Right, to its end and once more. Expected: each name; at the end the last name again.
3. Step Down through the three rows. The bottom row is shorter. Does where you land make sense?
4. Press A on a character. Expected: the game's announcer. Then X or Y for the costume.
5. Step Down to the player slots. Expected: the button you land on, with its value. Step Right to player 2's button and press A until it is a CPU. Expected: "Player 2: CPU," and a character's name.
6. Step Right to player 2's CPU level. Press A, step Right four times, press A. Expected: "Holding the slider", "Level 2" to "Level 5", "Released".
7. Choose the CPU's character. Step 5 told you which character the CPU got. Step to that portrait. Expected: its name and "player 2's coin". Press A. Expected: "Holding player 2's coin". Carry it to another portrait and press A. This is the step most likely to be awkward. Say how you would want it to work.
8. Expected once both have a character: "Ready to fight. Press Start." Press Start, then Start on stage select. A match begins.

## How it feels

9. Step quickly, several presses in a row. Does speech settle on the right target? Does the hand?
10. Hold a direction. Nothing repeats by itself in this version. Do you miss it?
11. Is the pause between a press and the move sound noticeable? Bothersome?
12. Change your character after choosing one: B calls the coin back. Is it clear what state you are in at each moment?
13. Step Down out of the portraits with the coin in your hand. Expected: "No character". Surprising, or fine once known?
14. Step up to the top bar and along it. Press A on Back. Expected: the VS. Mode menu.

## The stick and the D-pad together

15. Move the hand a little with the stick, then press a D-pad direction. Expected: the nearest target in that direction.
16. Push the stick during a glide. Expected: your stick wins, and hover announcements resume.

## Single-player modes

17. 1-P Mode, Regular Match, Classic. Choose a character, step to the difficulty arrows and the stock arrows, change both, start.
18. 1-P Mode, Training. Choose a character for yourself and for the CPU.

## Switched off

19. Launch with `MELEE_A11Y=0`. Expected: silence, and the D-pad does nothing on character select.

## Comments
