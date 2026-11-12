# 08 Play test on the Mac

Status: ready-for-human
Type: task
Blocked by: 06, 07

Spec: `.scratch/macos-port/spec.md`. The maintainer runs this with the friend, who has the Mac and VoiceOver. The build is the Mac zip from `ci-latest` (issue 06); first launch needs right-click > Open (VoiceOver: VO-Shift-M on the app, then Open) or `xattr -d com.apple.quarantine Melee.app`. Each launch writes `~/Library/Application Support/melee-pc/melee-pc.log` (Finder: Go to Folder, Cmd-Shift-G), replacing the last launch's; the friend copies it out after each check worth reading and sends it back.

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

## Comments

### 2026-11-12, code review of issues 01 to 07 (agent)

The review's plain fixes are in their own commit ("A11y: macOS port, review fixes"). Left for the maintainer, most severe first:

- **Quit waits for the voices without a limit.** `Voices::wait_in_turns` (`voices.cpp`) waits until the worker finishes, so a render AVSpeech never returns from (an audio device gone, a frozen synthesizer) would hang the game at quit; Windows had the same exposure with the bare `join`, and the Mac adds ways to stall. Options: leave it to check 12 above; or bound the wait (a few seconds), log it, detach the worker and skip `close`, which the bridge's comments already accept for Prism's context. Recommended: wait for check 12, bound it only if a hang is ever seen.
- **"Not installed" says the language twice on a non-English Windows.** With no en-US voice installed (a Finnish Windows with Heidi alone), a fresh start says "Voices Zira, English (United States) and Mark, English (United States) not installed, using Microsoft Heidi." on every start until a voice is picked; before issue 02 it said "Voices Zira and Mark not installed, ...". The commas run name and language together by ear. Options: say a shared language once ("Voices Zira and Mark, English (United States), not installed, ..."), or drop the language when no installed voice has the missing name in any language. Recommended: the first. (`VoiceSettings::missing`, `voice_settings.cpp`; pinned in `test_voice_settings.cpp`.)
- **`a11y.ini`'s comment on a Mac voice says "default empty".** A player editing the file by hand may write `your_voice.voice = empty`, a voice named "empty", and hear "Voice empty not installed" on every start. Only the Mac's empty defaults show it, until issue 09 sets them. Options: "default blank (the system's default voice)", or leave it, since issue 09 replaces the empty defaults. Recommended: leave it unless 09 keeps an empty default. (`describe`, `value_model.cpp`.)
- **The backend's defaults have a second home.** `Voices` takes the whole `VoiceBackend` and seeds each voice's wanted text from its defaults, which `VoiceSettings::attach` overwrites at once in the game; only the tests see the seed. Options: let `Voices` take the backend's name alone and start empty, leaving defaults to `VoiceSettings`; or keep it. A cross-module interface change, so not made in the review.
- **`macos-ci` in `a11y.yml`'s push trigger** is there only while `master` on GitHub lacks `a11y.yml`. Once `master` is pushed, drop it from the branch list and its comment, and from the `verify` skill's "Mac build" bullet.
