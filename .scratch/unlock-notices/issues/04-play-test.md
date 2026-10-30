# 04 Play test by ear

Status: ready-for-human
Type: task
Blocked by: 01, 03

The maintainer listens with NVDA and judges the wording of `.scratch/unlock-notices/spec.md`.

## What to try

- **The challenger.** Start the game with the challenger launch (issue 02). Play a VS match, or quit one from the pause (L+R+A+Start), and listen to the New challenger screen: is the line clear over the music, and is "A to fight" right though A works only after 3 seconds?
- **The fight and the unlock.** Win the fight and listen to "You've unlocked …". Lose once, too: is the silent return to character select clear enough?
- **A run of notices.** With the challenger launch's folder or a fresh one, the notices come as you unlock things. In an agent-style run (`--no-card`, which every driver uses) there are 8 after the title screen: is "A for next." on the first enough, and is leaving out the count right over a run of 8?
- **Trophies.** Do "You got the … trophy!" lines read well?

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue, and `CLAUDE.md`'s status line says the unlock notices and the New challenger screen were play-tested.

## Comments

### 2026-10-29, code review (agent)

The review of issues 01 to 03 (`b7b6b410..0e1ed9ad`) fixed what was plainly the agent's in its own commit. The items below are left for the maintainer.

- **Hear these in the play test.**
  - "A for next." is said on a visit's only or last notice too, such as the one notice after a challenger fight won; A then ends the scene. Is the key right there, or would other words fit better (`unlock_speech.cpp`, `kNextKey`)?
  - The challenged player's label ("… Jigglypuff. P2, A to fight.") is checked by unit tests alone, since the drivers press port 1 only. If two controllers are at hand, play a VS match with two humans and listen for it.
  - After a challenger fight lost, the only sign is character select naming itself. Is that clear enough?
  - The `--play` test folder starts on the default controller and keyboard bindings: only `a11y.ini` is copied, as issue 02 restricted. Should `keyboard_bindings.dat` and the `.controller` files be copied in too?
- **The hook carries the trophy branch** (`src/melee/if/ifprize.c`, `pc_a11y_unlock_notice(a == 0x3E ? 0x4A : un_803F9D48.x6)`). This is the only hook argument in the decomp layer with a condition, and it copies the decomp's 0x3E / 0x4A knowledge into a base port file. Alternative: pass the raw `(a, un_803F9D48.x6)` and map 0x3E to 0x4A in fork code. This changes the hook's interface. Recommended: do it, since fork code should decide what to say.
- **The player label is built in three places**: `unlock_speech.cpp` (`challenger`), `match_announcer.cpp` and `results_speech.cpp` with `results_names.cpp`, each as "name tag, else P and the port". They already differ: results logs trouble decoding a name tag, and the new line drops it silently. Alternative: one shared helper. This is a refactor across modules. Recommended: do it in its own commit when one of the three next changes.
- **The prize glyphs ignore the language.** `UnlockSpeech` always hands the decoder SdPrize's glyphs (é, II). A US disc whose save holds Japanese as its language loads `SdPrize.dat` instead (`ifprize.c`, `setArchive`), whose font may put other characters at 0x4000 and 0x4002. Alternative: give the glyphs only while `lbLang_IsSavedLanguageUS()`, through a `game_access.c` query. Recommended: fold this into region work, since the spec leaves every setup but the US one unchecked.

### 2026-10-30, review items fixed (agent)

The maintainer asked for three of the review's items to be fixed as recommended:

- **The hook carries the trophy branch** (`8d8864ce`). `pc_a11y_unlock_notice(unlock, text_id)` now passes the unlock number and the string bound to the frame's message as `un_802FE918` has them, one plain line in `ifprize.c`; `UnlockSpeech::notice` reads the composed placeholder 0x4A for unlock number 0x3E. A unit test checks the trophy notice against the first part the frame binds; `unlock_notices.drive` still hears the three trophies.
- **One player label** (`13a79c2a`). `player_label` in `game_text.hpp`, beside the shared game text step, is the name tag's words or "P" and the port, a tag's decoding trouble logged once. Results, the match announcements and the New challenger line use it, each saying the same words as before; the match announcements keep their own CPU slot, "teammate" and leaving the tag out online, and now log a tag's trouble too, as the New challenger line does.
- **Font glyphs on other tables** (`29d1ea6b`). A dump of each table found SdMenu and SdSlChr with 0x4000 é and 0x4002 II, SdRst with 0x4000 é alone (its 0x4001 to 0x4006 spell Japanese match types), and SdMsgBox with none. The main menu tree's reader (the rules screens in both hosts too) and results each hand over their table's; character select, stage select and training decode no game text. Results now says "Pokémon KO". No drive script reaches a string with these glyphs, so the unit tests carry them; no expected line in the `verify` skill changed. Two Sound Test song names in SdMenu (259, 260) put the II straight after "Kingdom", unread so far.

The language item (the prize glyphs given whatever the saved language) stays for region work, and applies now to the menu and results glyphs as well.
