# 03 Rules screens inside character select, and the rules header

Status: ready-for-agent
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
