# 04 Play test by ear

Status: resolved (2026-10-14)
Type: task
Blocked by: 03

The maintainer plays the rules screens with NVDA and judges the wording of `.scratch/rules-screens/spec.md`.

## What to try

- From the main menu tree: VS. Mode > Custom Rules. Every row and a few values on each; Rules from Time to Stock and the renamed row below; a row that opens a screen.
- Additional Rules: every row, the self-destruct values, Random Stage.
- Item Switch: moving down both columns, switching columns, the frequency strip and its hint, toggling an item off and on.
- Random Stage: moving, toggling, turning off every stage but one and trying the last.
- Coming back from each sub-screen, and B and Start from Custom Rules.
- From character select: the rules header's text with the hand on it, opening the screens, changing a rule, returning, and the header's new text.
- Online, if a friend is at hand: either player changing a rule in a session. Otherwise noted as untested.

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue, and CLAUDE.md's status line says the rules screens were play-tested.

## Comments

### 2026-10-11, code review (agent)

Review of 8ce99d1, 94b1d66 and b0eb319; fixes in 6e64794. Left for the maintainer:

- **The header's text in a failed glide and a blocked step.** "Could not reach Rules: 3-man survival test! Fox" and "Rules: 2-minute KO fest! Not while holding a coin" repeat the whole rules header where "Rules" would do. Options: keep it everywhere, or name the header only when the hand reaches it or a step names it. Recommendation: judge by ear here; if it drags, say "Rules" alone in those two messages (`target_words` in `css_speech.cpp`).
- **The hand-over to the menu reader has no unit test.** The latch on pending exit 3, the gate on 5 and the forget on each visit live in `pc_a11y_css_frame` (`hooks.cpp`); only `css_rules.drive` covers them. Options: move them into a small class in `src/pc/a11y/` with a test, or leave them. Recommendation: leave until the gate changes again.
- **PAL rules header words.** PAL discs compose the header from other pieces (`css_pal_header` in `mncharsel.c`); the fork says the English NTSC words. Recommendation: leave until a PAL disc is checked, as with the SdSlChr remap.

### 2026-10-14: play-tested by ear

The maintainer played everything above with NVDA except online and reports that it works nicely. No new issues. The three points left by the code review were not raised, so they stay as they are. Online, either player changing a rule in a session, is untested; it moves to issue 05 for a later test with a friend.
