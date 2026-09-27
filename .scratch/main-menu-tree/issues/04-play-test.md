# 04 Play-test the main menu tree by ear

Status: ready-for-human
Type: task
Blocked by: 01, 02, 03

Maintainer, with NVDA. Report what you hear for each step; the agent then reads the `[a11y]` lines of `melee-pc.log` against the report. Expected wording is in `.scratch/main-menu-tree/spec.md`.

Anything unexpected becomes a new issue in this directory. Words that read badly are corrected in the string table, which is what the spec left to this test.

## Getting in

1. Start the game from the launcher with a normal launch. Expected: "Opening movie. Press Start to skip."
2. Press Start at once. Expected: "Title screen. Press Start."
3. Press Start. Expected: "Main Menu. 1-P Mode. Solo Smash!" If an unlock notice comes first, you hear "Unlock notice. No speech yet."; A dismisses each one.
4. Start the game again and let the opening movie play for a while before pressing Start. Near its end, Start goes straight to the main menu. Is it clear enough what happened?

## The tree

5. Down through the main menu and past its end. Each entry gives its name and description; the wrap to the first entry gives nothing extra. Do you miss a sign that the list wrapped?
6. Hold Down. Does the speech keep up, and does it settle on the right entry when you let go?
7. Confirm into each of the five screens and Back out again. Going in, the title repeats the name you just heard. Helpful or tiresome?
8. Press Down right after Confirm, while the screen still slides in. The right entry should be spoken.
9. Go down to 1-P Mode, Regular Match, then press L+R+Start. Expected: "Main Menu. 1-P Mode. Solo Smash!"
10. VS. Mode, Online. Do the base port's labels and descriptions read well?
11. Listen for words that read badly: "1-P Mode", "VS. Mode", "Slo-Mo Melee", "Home-Run Contest", "Misc. Records". Check the same ones on a braille display if one is at hand.
12. Descriptions are always spoken. Too much, once the menu is familiar?

## Leaf screens

13. Options, Rumble. Expected: "Rumble. No speech yet." Back returns to Options on Rumble.
14. Options, Sound. Change the channel both ways, move to Volume, move the balance both ways and hold a direction. Is "15 toward music" the right way to say it? Put it back to the centre and to Stereo.
15. Press Left on Stereo, and Right at the far end of the balance. Expected: silence, and no sound from the game.
16. Options, Screen display. A toggles. Put it back to On.
17. 1-P Mode, Stadium, Multi-Man Melee. Left and Right through the six choices.
18. Are the key hints at the opening of the three screens right, or in the way?

## Leaving and coming back

19. VS. Mode, Melee. Expected: "Character select. No speech yet." Hold B to go back. Expected: "VS. Mode. Melee." with its description.
20. 1-P Mode, Stadium, Multi-Man Melee, confirm a choice, then go back from character select. Where do you land, and is it spoken?
21. On the title screen, wait about ten seconds without pressing anything, until the attract loop starts, and let it run a few minutes. The title and the movies are named on each pass. Too much?

## Speech itself

22. With the game on the main menu, restart NVDA and move the cursor. Does speech resume? This case was carried over from the speech play test, where it could not be tested.
23. `MELEE_A11Y=0` launch. Expected: silence throughout, and the log marks each announcement `(off)`.

## Comments
