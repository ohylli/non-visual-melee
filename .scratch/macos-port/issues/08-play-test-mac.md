# 08 Play test on the Mac

Status: ready-for-human
Type: task
Blocked by: 06, 07

Spec: `.scratch/macos-port/spec.md`. The maintainer runs this with the friend, who has the Mac and VoiceOver. The build is the Mac zip from `ci-latest` (issue 06); first launch needs right-click > Open (VoiceOver: VO-Shift-M on the app, then Open) or `xattr -d com.apple.quarantine Melee.app`. Set `MELEE_LOG_FILE` to a known path when launching from Terminal, since a launched app's working directory is not the bundle; the friend sends the log back.

## Speech

1. **VoiceOver on**: hear "Non-Visual Melee ready". The log names the backend Prism chose; record it.
2. **VoiceOver off**: hear Apple's voice instead; record that backend's name.
3. **Interrupting**: in a menu, move quickly through several entries. Does each new announcement cut VoiceOver off, or does VoiceOver queue them?
4. **Unasked speech**: does VoiceOver read the game window itself (its title, "window") on top of the announcements? Note anything said that the player did not ask for.
5. **Braille**, if a display is at hand: do the announcements reach it?
6. **The launcher and the port menu**: read as on Windows?

## Voices

7. **They start**: the log's voices line names AVSpeech, the number of voices and the test line's render time. Record the time.
8. **Personal Voice**: on macOS 14 or later, does a permission dialog come up as the voices start, and does it take focus?
9. **Choosing voices**: first, which voice both voices speak with before any is chosen: the build's default is Prism's, the first voice Apple lists in the system's language, not the Spoken Content setting's (the log's `voice yours speaks with` line names it). Step the Voice rows; is each voice said with its language when there are several? Pick two voices that tell you and the opponents apart by ear, and record each by name and language (the saved form in `a11y.ini`, `Name (xx-YY)`): they become the defaults (issue 09).
10. **In a match**: are the announcements prompt, or late by more than a moment? Do they wait through a scene loading?
11. **Audio ducking**: while VoiceOver speaks, do the game, the voices and the cues get quieter? If so, does VoiceOver's own setting (VoiceOver Utility, Sound) change it?
12. **Quit**: quit with the voices open, and again while a line plays (a voice's Preview), each from the port menu's Quit game and with Cmd-Q. The game must close at once (issue 07's turn); a hang here leaves `voices` in the log's last lines without `speech shutdown`.

## Cues and the rest

13. **The cues**: the position cues, the gap warning and the ledge warning on Battlefield and Final Destination, as on Windows; the floor warnings are the check of issue 04.
14. **Steering** on character select and stage select.
15. **The connection checker**, already confirmed, once more from this package.
16. **A missing Prism dylib**: remove it from `Melee.app/Contents/Frameworks` and launch. Expected: a dialog that the app cannot be opened. Does VoiceOver read it?

## Done when

Each numbered check has its answer recorded in this file's comments, and issues for anything that fails.
