# 03 Rules screens inside character select, and the rules header

Status: resolved (2026-10-11)
Type: task
Blocked by: 02

The slice of `.scratch/rules-screens/spec.md` that brings the four screens' speech into character select, and says the rules header's text. Read the spec, issue 01 and 02's comments, and the character select spec's notes on the pending exit first.

## Machinery

- **Hosting.** In `hooks.cpp`, while character select's pending exit says the rules screens are open, feed `a11y_game_menu_state` to the menu reader from `pc_a11y_css_frame`. No new base port hook. Forget the reader's state each time the screens open, so a second visit is an arrival. Character select speech drops its "Custom Rules. No speech yet."; name entry keeps its notice.
- **String table.** In `GS_CSS` the font slot holds SdSlChr. The string reader accepts it, with PAL remapping by that table. Check, with the text dump reaching `GS_CSS`, that every number the rules screens use holds the same text there (spec, open question 3).
- **Rules header.** The hand reaching it says "Rules: " and the header's text (spec, open question 4 on where to read it). Camera Mode's header as drawn.
- **Stale state.** `cur_menu` keeps its last value after the screens close; gate on the pending exit.

## Speech in this slice

Wording is in the spec, "Speech: character select". The four screens speak as in the main menu tree. Returning speaks character select's opening, as today.

## Done when

- `cmake --build build` passes, and the unit tests pass: `test_css_speech`'s rules test changed for the hand-over, and the rules header's text for Time, Stock (one and several), Coin, Bonus, no time limit and Camera Mode.
- A drive script opens the rules screens from character select's rules header, changes a value on Custom Rules, visits each sub-screen, returns, opens them a second time and hears the opening again, and waits on each expected line. `css_slots.drive` and `sss_skipped.drive` wait on the new opening in place of "Custom Rules. No speech yet.". Every character select script still passes.
- The online drive (`css_net.drive` or a short `drive_net.py` run) still passes; opening the rules screens online is not required.
- `python tools/check_style.py` passes.
- The `verify` skill updated.
- ADR-0003's note on a second call site for `GS_CSS` is resolved; the primer records the SdSlChr finding.
- CLAUDE.md: the character select lines no longer say the rules screen has no speech, and the status line for the rules screens names both hosts.

## Comments

### 2026-10-11, implementation (agent)

Implemented; hearing it is issue 04.

- **Hosting.** `pc_a11y_css_frame` in `hooks.cpp` records whether the pending exit last asked for the rules screens (3) rather than name entry (4), recorded even in re-run frames like the exit, and while the pending exit is 5 after a 3 it feeds `a11y_game_menu_state` to the menu reader after character select's own reader, which says nothing while the screen is away and still needs those frames to see the return. The frame with pending exit 3 is not fed: `cur_menu` there is still the menu tree's last screen, set to Custom Rules only by the opening function later in that frame. The first frame fed after any frame not fed forgets the menu reader, so every visit is an arrival with the hint; tracking "fed last frame" rather than the 3 itself keeps a visit whose 3 fell in a re-run frame an arrival too. The refused flag is taken before the speech gate, as in the menu frame; an ad hoc run from character select that turned every stage off but Rainbow Cruise heard "At least one stage must stay on." there. `CssSpeech` drops its "Custom Rules. No speech yet."; name entry keeps its notice. No new base port hook.
- **String table (spec open question 3), answered for NTSC-U.** The text dump now logs each table font slot 0 holds once per run, so it reaches `GS_CSS`. `SdSlChr` strings 0 to 73 equal `SdMenu`'s 0 to 73 character for character: the item columns, stage names and every rule description the screens use (0x26 to 0x41), so `menu_string()` accepts `SIS_SelCharData` as well and the descriptions are read in both scenes. From 74 on `SdSlChr` has its own strings: the rules header's scratch line and pieces. PAL: `pc_region_sis_index` shifts the two tables differently (all of `SdMenu` from 2, `SdSlChr` only from 28 to 74); the fork remaps by the loaded table's symbol, as the game's own binding does, and the rule descriptions fall in the range both shift by 2. No PAL disc was checked. The primer has it under "The rules screens on character select".
- **Rules header (spec open question 4), answered: composed from `GameRules`.** `rules_header()` in `css_names.cpp` builds the words `mnCharSel_8025BD30` composes on the English screen: "2-minute KO fest!", "An endless KO fest!", "3-man survival test!", "1-life survival test!", "5-minute coin fest!", "99-minute skills test!", Camera mode's "Take Snapshots!". The pieces were read from the `SdSlChr` dump and a screenshot after a change to Stock showed "3-man survival test!". The drawn text sits in a scratch string the game rewrites in place, digits written as glyphs, at another number on PAL; composing keeps the words testable and independent of that layout. The state gains `A11yCssState.rules`, and Camera mode's match type is mirrored as `A11Y_CSS_VS_CAMERA` with a compile-time check. Japanese ordering is not followed; the fork's words are English throughout.
- **Tests.** `test_css_speech`: the rules screens hand-over (character select says nothing on opening them, its opening on return, a rule changed meanwhile) and the header for time at 2, 1 and none, stock at 3, 99 and 1, coin at 5 and none, bonus at 99 and none, and Camera mode. `tools/a11y/css_rules.drive` (about 40 s) opens Custom Rules from the header, changes Rules to Stock, visits Item Switch, Additional Rules and Random Stage and returns from each, goes back to character select, hears "Rules: 3-man survival test!", opens the screens a second time to the opening with the hint, and puts Rules back. `css_slots.drive`, `sss_skipped.drive` and `css_setup.drive` wait on the header's text and the new opening; `sss_skipped.drive` now waits on each row it passes.
- **Verification.** Build, `check_style.py`, unit tests (only `launcher_data` fails, the known Windows failure), a bounded `title` run, all sixteen menu drive scripts and `results.drive` pass first time, no press lost, and the five `css_net.drive` runs and `lan_lobby.drive` pass. The verify skill's absence check on `sss_skipped.drive` now greps `"Stage select\.` with the quote and full stop, since Custom Rules' "Stage selection" row would otherwise count.
- **Docs.** ADR-0003's second call site is resolved; the primer has a section on the rules screens inside character select (the gate, `SdSlChr`, the header); CLAUDE.md's main menu tree line names both hosts and the character select line the header and the hand-over; the verify skill lists `css_rules.drive`.
- **Left open.** Score Display's visibility online (why the earlier run with `x186C` pinned showed no row) was not looked into; opening the rules screens online is not driven. The rules screens' words in character select are the main menu tree's, so issue 04 can test them from either host. For the play test: the header is said only when the hand reaches it, not in the opening; a D-pad step onto it says the same words, and so does a glide that fails to reach it ("Could not reach Rules: 2-minute KO fest!").
