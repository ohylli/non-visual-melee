# 04 Drive scripts, verify skill and status line

Status: resolved (2026-10-27)
Type: task
Blocked by: 03

Spec: `.scratch/results/spec.md`, "Tests" and "Docs".

- **The offline scripts** cover:
  - Stock, with "you" (debug match, `MELEE_DEBUG_VS=cpu MELEE_DEBUG_VS_STOCKS=1`);
  - No Contest (`MELEE_DEBUG_VS=cpu` without stocks, then L+R+A+Start);
  - four CPUs and nobody "you" (`MELEE_DEBUG_VS=cpu4 MELEE_DEBUG_VS_STOCKS=1`).

  Settle whether a real timed or team match through the menus is worth its two minutes in the menus level; if not, the unit tests carry those forms.
- **The `verify` skill**: the new scripts, the online run and their expected lines, at the levels they belong to.
- **`CLAUDE.md`**: one "Accessibility status" line for results, naming the play test as waiting (`.scratch/results/issues/05-play-test.md`).
- **`docs/a11y/native-menus.md`**: the screen inventory's results line points to "Results in detail", and the section is complete.

## Done when

The menus and online levels of the `verify` skill pass with the new scripts.

## Comments

### 2026-10-27, implementation (agent)

Implemented; three new offline scripts beside `results.drive`, all in `run_level.py menus`.

- **`results_no_contest.drive`** (`MELEE_DEBUG_VS=cpu MELEE_DEBUG_VS_STAGE=31`): Start pauses the clockless Time match past GO and L+R+A+Start ends it; Start skips the announcer's wait. Heard: `"Time Battle. No contest. Link, P1, you, 0 KOs, 0 falls, 0 SDs, total 0. Mario, P2, 0 KOs, 0 falls, 0 SDs, total 0. Any button shows the stats. Start when ready."`, Z without a place (`"Mario, P2, 0 KOs, 0 falls, 0 SDs, total 0."`), A on Stats, the last of two pages, silent, the edge `"Link, P1, you"`. `run_level.py` fails it on any spoken place, `"Bonuses.` or ready line.
- **`results_cpu4.drive`** (`MELEE_DEBUG_VS=cpu4 MELEE_DEBUG_VS_STOCKS=1 MELEE_DEBUG_VS_STAGE=23`). On Final Destination, the debug match's own stage, the four CPUs took up to 3.5 minutes; probes of twelve stages found Poké Floats ending in about 40 seconds in every run, the others in 2 to 4 minutes (Icicle Top not within 7). The outcome varies from run to run, so the script pins the forms: the winner and four players by place, controller 1 opening and paging P1's panel, a step over P1's rows, switches over all four to DK's edge and back to `"Link, P1"`, Z on DK and on Link, A to Stats and Bonuses and silent at the end, B back, and Start leaving with no human. `run_level.py` fails it on any line saying ", you" or a ready.
- **`results_teams.drive`**, through the menus: Teams on, Pichu for player 1 (red), player 2 a CPU (it joins red) turned blue at its team button, one stock from the rules header, Princess Peach's Castle, Pichu walks off. Heard: `"Stock Battle. Winner: Blue team. 1st Zelda, P2, Blue team, 0 KOs, 0 falls, 0 SDs, not KO'd. 2nd Pichu, P1, you, Red team, 0 KOs, 0 falls, 1 SD, KO'd at 0:02. ..."`, `"Zelda, P2, 1st, Blue team, ..."` and `"Pichu, P1, you, 2nd, Red team, ..."` with Z. A screenshot agreed (the "BLUE TEAM" banner, the columns' colours). The first driven run of a team match: until now the team words were unit-tested alone.
- **The timed and team match decision.** A team match is in the menus level: with one stock and a walk-off it takes about 40 seconds, not two minutes, and it is the only run that reads the team fields from the game. A timed match played to its clock is not: the No Contest run already reads Time's last row from the game, and a nonzero total's sign is a number the unit tests cover with every sign; waiting out the clock would make it the level's longest script by far.
- **Findings** (`native-menus.md`, "Results in detail"): ties met at last, in the four-CPU match: three players sharing 2nd behind the winner, or two sharing 2nd and the next shown 4th, said as the screen shows them; with no human, any connected controller's first press opens the panels, CPU panels are held un-ready, and Start from a connected controller ends results with the confirm sound (the fork says nothing); a cancelled match's screen; the team colours. The summary draws the points beside its KOs, Falls and SDs labels ("+1", "-1", "-1"), in Stock matches too; the opening does not say them (for the play test to judge).
- **`results.drive`**: its Bonuses wait now accepts a first bonus whose name holds a comma ("Run, Don't Walk"), which `[^,"]+` refused.
- **Docs.** The `verify` skill: the three scripts' lines, the decision on the timed match, and their cue lines with `MELEE_A11Y_CUES=1`. `CLAUDE.md`: the Results status line, waiting for issue 05, and the LAN play line's quick chat on results. `native-menus.md`: the findings above.
- **Verification**, menus and online levels with `MELEE_A11Y_CUES=1`: build with the "Rollback sections" line, style, unit tests 31 of 32 (`launcher_data`, known), a bounded `title` run, `run_level.py menus online` 33 of 34, `direct_lost` timing out waiting for the match hello over the internet and passing on an unchanged rerun.
- **Left open.** PAL is still unchecked. Another human's un-ready offline stays unit-tested alone. The wording is the play test's (issue 05).
