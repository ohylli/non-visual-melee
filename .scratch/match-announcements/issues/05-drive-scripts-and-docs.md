# 05 Drive scripts, the online run, the verify skill, the docs and the status line

Status: resolved (2026-10-28)
Type: task
Blocked by: 02, 03, 04

Spec: `.scratch/match-announcements/spec.md`, "Tests" and "Docs".

## What to do

- **The drive scripts.** Keep the earlier issues' runs as scripts: the debug match with stocks (damage and stocks), a timed match (the time left), `MELEE_DEBUG_VS=cpu4` (labels with no "you").
- **The online run** through `tools/a11y/drive_net.py`, fork against fork and against the base port build (`--base-b ../melee-pc-base/build`): both copies say their own and the other's damage and stocks, unlabelled, and the session stays in sync.
- **The `verify` skill.** The scripts, the online run and their expected lines, at the levels they belong to.
- **The docs.** `docs/a11y/gameplay-state.md` complete with what the issues found; `docs/a11y/speech.md`'s "Voices" section points to what speaks through them.
- **CLAUDE.md.** One "Accessibility status" line, naming what still waits: the play test, `.scratch/match-announcements/issues/06-play-test.md`.

## Done when

- The `verify` skill's levels run the new scripts and the online run, and they pass.
- The docs and the status line are in place, with no values or counts that tuning would change.

## Comments

### 2026-10-28, implementation (agent)

Implemented; the match announcements' drive runs are scripts in the menus and online levels, and the docs and the status line are in place.

- **The scripts.** Four new ones in `run_level.py menus`, each run muted so the voices render and log their `plays` lines, `run_level.py` setting the debug match's switches: `match_damage.drive` (the debug match against an attacking CPU, `MELEE_DEBUG_VS=cpu`, two stocks: Mario's damage in the opponents' voice and Link's in yours, unlabelled, about 20 s); `match_thresholds.drive` (the same match with both damage rows stepped to Thresholds in the port menu on the title screen, each saved and checked with the `ini` step, about 30 s; `run_level.py` gains a `CHECK` table and `threshold_crossings`, which fails the run unless each figure said in a voice crossed a multiple of the damage interval, read from the run's `a11y.ini`, since the last said, a lower figure counting from 0 as after a KO); `match_stocks.drive` (Mario standing still, three stocks: `voice opponents: "2 stocks"` as he falls, then `voice yours: "2 stocks"` as Link walks off, about 30 s); `match_time.drive` (through the menus, the time limit stepped from 2 minutes to 1 on Custom Rules from the rules header, a random stage: `speak queue (muted): "30 seconds left"` 30 s after GO, then it quits, about a minute; `run_level.py` fails it on any line saying minutes left). Labels with no "you" (`MELEE_DEBUG_VS=cpu4`) went into `results_cpu4.drive` rather than a script of their own, since it already plays that match: it waits for a damage line labelled by slot, and `run_level.py` now fails it on any line in your voice. An unlabelled line is checked by the scripts' anchored waits (`voice opponents: "\d+"$`), which a label would fail.
- **The timed match: one minute, chosen** as the coordinator's note preferred: it says only "30 seconds left", and the absent "1 minute left" checks that the starting minute is never said. The whole minutes stay the unit test's; the `verify` skill says how a copy of the script without its Custom Rules steps plays the two-minute match by hand (about two and a half minutes with TIME!). The menus level now takes about 130 s side by side, 33 scripts.
- **The online run.** New `match_net.drive` in `run_level.py online` (`drive_net.py --muted`, ports 42170): Pichu for player 1, two stocks, Final Destination by D-pad (a base port b presses Start for Random; the default seed takes player 1's pick). Pichu's Thunder Jolts hit player 2 from the start positions and give Pichu recoil, so both damage lines come from player 1's presses alone; then Pichu runs off the left edge and player 2 off the right, each "last stock" said on both. Each copy says its own lines in your voice and the other's in the opponents' voice, unlabelled, and the recordings agree. Fork against fork and against the base port build (`../melee-pc-base/build`, at `0fb0af3`, the commit `master`'s last base merge took) both pass; a base port b says nothing and a says both players' lines.
- **Findings from writing it.** A dash-and-smash fight against a standing player pushes them off Final Destination's right edge, and the respawns can come down on the other side (player 2 landed left of Pichu in one probe), so a script that fights after a KO cannot count on directions: hence the Jolts first and the edges after. Pichu's forward smash and Thunder hurt Pichu too. Ranked's forced countdown is out of the drivers' reach, so no online run says the time left.
- **The `verify` skill.** "Match drive scripts" replaces the by-hand runs with the four scripts and their expected lines, names the quick level's set for an announcer change, and keeps the two-minute match as a by-hand note; `results_cpu4.drive`'s entry has its labelled line and the new absent line; the online level and "Online runs" have `match_net.drive`.
- **The docs.** `docs/a11y/speech.md`'s "Voices" opens with "What speaks through them", pointing to the announcer, the spec and `gameplay-state.md`'s "The HUD". `docs/a11y/gameplay-state.md` already had the answers to the spec's six open questions (issues 01 to 04); it gains the Multi-Man wireframe's fall to 0 (issue 03's finding), the online run's result, and an answer for the match announcements under its open question on predicted ticks. CLAUDE.md gains the status line, waiting on issue 06, the Voices line no longer says they speak only their samples, and the Accessibility settings line lists the match announcements' rows. No values or counts that tuning would change.
- **Verification**: build, style, unit tests 35 of 35 (`launcher_data` passed this time); `run_level.py menus online`: online 8 of 8, menus 25 of 33 on the first pass, the eight failures (`css_slots`, `match_stocks`, `match_stock_steal`, `match_time`, `port_menu_a11y` and the three launcher scripts) all with lost presses, waits timing out, or `aurora: Surface texture is Error` in five of them, the game window's surface lost; the eight passed on an unchanged rerun, and the whole menus level then passed 33 of 33. `run_level.py online --base-b ../melee-pc-base/build` 7 of 7, `match_net` among them.
- **Left open.** The surface errors of that first menus pass, now among the `verify` skill's flakes: they came on one pass only, with no change between it and the passing ones. The play test, issue 06, with the Multi-Man wireframes, the stock line a damage line can replace, Training's second "21" and 3-Minute Melee's countdown from the earlier issues.
