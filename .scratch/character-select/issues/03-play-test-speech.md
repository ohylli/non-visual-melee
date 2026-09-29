# 03 Play-test character select speech by ear

Status: ready-for-human
Type: task
Blocked by: 02

Maintainer, with NVDA. Report what you hear for each step; the agent then reads the `[a11y]` lines of `melee-pc.log` against the report. Expected wording is in `.scratch/character-select/spec.md`, section "Stage 1: announcements".

This stage has no steering yet, so the hand moves with the stick only. Finding a given target is not expected to be easy. The test is about what is said, and whether it would serve a player whose hand is being moved for them.

Anything unexpected becomes a new issue in this directory. Words that read badly are corrected in the string table.

## Getting in

1. Main menu, VS. Mode, Melee. Expected: "Character select. Player 1, no character."

## Portraits and the coin

2. Push the stick up gently. When the hand reaches the portraits, the coin jumps into it. Expected: "Holding your coin", then a character's name as the coin reaches a portrait.
3. Move slowly across a row. Each portrait is named as the coin enters it. Does the speech keep up at a gentle tilt? At a full push?
4. Press A on a portrait. Expected: the game's announcer says the name, and speech says nothing. Is it clear that the character was chosen?
5. Press X or Y. Expected: "Costume 2".
6. Move the stick over the portraits with the coin placed. Expected: silence.
7. Press B. Expected: "Holding your coin". Move to another portrait, then press B again. Expected: "Back to" and the first character.
8. Press B to take the coin, then move the hand all the way down. Expected: "No character".
9. Listen for names that read badly: "Dr. Mario", "DK", "C. Falcon". Check them on a braille display if one is at hand.

## Player slots

10. Move the hand down to the player slots and sideways along them. Buttons are named with their value as the hand enters them: "Player 2: closed".
11. Press A on player 2's button. Expected: "Player 2: human" or "Player 2: CPU," with a character's name, depending on where it started. Press A until it is a CPU.
12. Find the CPU level slider of player 2, to the lower part of its slot. Expected: "Player 2 CPU level: 1". Press A, move the stick right, press A. Expected: "Holding the slider", the levels as they change, "Released".
13. Once both players have a character. Expected: "Ready to fight. Press Start."

## The top bar

14. Move the hand to the top edge and sideways along it. Expected: "Teams: off", "Rules", "Back".
15. Press A on Rules. Expected: the rules screen's name and "No speech yet." B returns. Expected: the opening announcement again.

## Leaving

16. With Ready to Fight announced, press Start. Expected: "Stage select. No speech yet. Press Start for a random stage." Press Start. A match starts.
17. After the match, back on character select: the opening announcement names the character you had.

## Questions

18. Is "Holding your coin" worth saying each time, or noise?
19. Is the opening announcement enough, or should it say more, such as the mode or the other player slots?

## Comments
