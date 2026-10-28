# 05 Play test by ear

Status: ready-for-human
Type: task
Blocked by: 04

The maintainer plays results with NVDA and judges the wording of `.scratch/results/spec.md`.

## What to try

- **The opening.** Finish a 1v1 Stock match, then a Time match, and listen to the opening: its timing after the announcer, its length, "not KO'd", and the keys on the first results only.
- **Teams and No Contest.** If convenient, a team match and a No Contest.
- **Paging.** Open the stats and page with A and B: the page names and the silent ends.
- **The reading cursor.** Step down every page with the D-pad and switch players on each:
  - Is "Mario, P2, 41%" enough on Stats?
  - Do the KOs and falls headings as rows work?
  - Do the decoded stat labels read well with their spaces and units?
  - Do the bonus names ("No R 4 U") read well?
- **Z.** Press Z on a few players.
- **Online.** Play a match against a friend or a second copy, and ready in turn. Is "Ready, waiting for your opponent." and "Your opponent is ready." right? Did the two points proposed in writing hold up: another player's un-ready, and the reading cursor while ready? Does an opponent's early press leave your opening whole, the panels' opening following it?

## Done when

The maintainer's findings are recorded below, each either fixed or filed as its own issue, and CLAUDE.md's status line says results was play-tested.

## Comments

### 2026-10-27, code review (agent)

The review's fixes are in their own commit ("Results: review fixes"). Left for the maintainer, most severe first:

- **Another player's press cuts off your opening.** The panels' opening interrupts whoever's press opened them (`ResultsSpeech::tick`), as the spec's "any press interrupts the opening" has it. Online a sighted or base port opponent often presses within a second or two, which cuts your opening after the winner and drops the places, your own included. Options: interrupt only on your own press and queue otherwise (the snapshot would need an "any new press" bit for the local port), or keep it. Recommended: queue unless the press was yours.
- **A page change does not say whose rows it reads.** With the reading cursor switched to another player, A says "Stats. KOs, 0." with no name, which sounds like your own. Options: add the player when the cursor is not on you ("Stats. Mario, P2, KOs, 0."), or keep the cursor's player silent. Recommended: add the name.
- **A Start inside the panels' slide does nothing, silently.** The panels' opening is said as state 3 begins, but the panels take input only after their slide; a quick Start after a short opening can fall inside it, and the game ignores it. Options: say the panels' opening once they take input (state 3's sub-phase, dropped from the snapshot as unused, would come back), or listen for it here first. Recommended: listen for it in this play test.
- **Shared sentence helpers.** `add_sentence`, `add_part` and `end_sentence` in `results_speech.cpp` repeat `menu_speech.cpp`'s, as does the decode-and-log-trouble step around `spoken_text`. Options: one helper beside `spoken_text` in `game_text.cpp` for both readers, or leave the copies. Recommended: share them when a third reader needs them.
- **"Results in detail" is long.** About three times the other screens' sections in `docs/a11y/native-menus.md`, after the review's trims. Options: its own primer behind a one-line pointer, or keep it. Recommended: keep it until results' end screen (Classic, Adventure, All-Star) adds to it.

### 2026-10-28, another player's press (agent)

The review's first point is fixed as recommended: the snapshot's `pressed_any` says whether the local port pressed anything on the panels' first tick, and the panels' opening interrupts only after your own press; after another player's it is queued behind what is being said. `results_net.drive` now checks both sides: interrupting on a, whose press it was, queued on b. "What to try" asks for it under Online.
