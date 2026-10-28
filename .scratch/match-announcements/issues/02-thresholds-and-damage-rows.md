# 02 Threshold mode and the damage rows

Status: resolved (2026-10-28)
Type: task
Blocked by: 01, `.scratch/voices/issues/01-choice-values.md`

Spec: `.scratch/match-announcements/spec.md`, "Damage" and "The settings rows".

## What to build

- **The rows**, after the Match announcements switch: "Your damage" and "Opponents' damage", choices of Off, Every change and Thresholds, Every change by default; "Damage interval", a slider from 10 to 100 in steps of 10, 20 by default, shared by both. Descriptions are the implementer's first proposal.
- **Off** says no damage for that side.
- **Thresholds.** The settled damage is said only when it has crossed a multiple of the interval upward since the last figure said: 35 to 52 at 20 says "52"; 35 to 85 says "85" once. A fall is silent but lowers the figure the next crossing counts from: 85, healed to 55, hit to 65 says "65". A KO resets it to 0, silently.
- **Teammates** follow "Opponents' damage".
- **Changes take effect at once**, partway through a match too.

## Done when

- The announcer's unit test covers each mode for each side, one and several crossings, a fall then a crossing, a KO, a change of interval partway through, and a teammate following the opponents' row.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.

## Comments

### 2026-10-28, implementation (agent)

Implemented; each side's damage can be said on every change, only past thresholds, or not at all, from three new rows.

- **Where things live.** `match_announcer.hpp/.cpp`: `DamageMode::thresholds`, `MatchAnnouncementValues::damage_interval`, and `owed()`, which says whether the figure shown is to be said once it settles by the mode in force; `tick` and `finish` both ask it. `match_announcement_settings.hpp/.cpp`: the choices `match_announcements.your_damage` and `opponents_damage` (fixed options Off, Every change, Thresholds, in `DamageMode`'s order, saved by their names) and the slider `damage_interval` (unit `%`, so it reads "20%" as the HUD's damage does), registered after the switch and before the settle time, so the file keeps that order too. No hook or `hooks.cpp` change: the values were already handed in each tick.
- **The rule as built.** A crossing is counted in whole multiples, `shown / interval > said / interval`, so reaching a multiple crosses it (39 to 40 at 20). A settled rise short of a crossing leaves `said` where it was, as the spec's "since the last figure said" reads; a settled fall lowers it silently; a KO zeroes it as before. The interval and the mode are read each tick, so a change partway through counts from the last figure said at the new spacing (21, then 35 at 20 is silent, and the interval set to 10 says "35" at once). At GAME! only a figure still owed is logged as dropped, so a rise short of a crossing is not.
- **A finding worth keeping.** Switching a side from Thresholds to Every change says a settled figure that crossed nothing, once, since it differs from the last said. That follows Every change's rule and was left so.
- **Descriptions (first proposal).** Your damage: "How your damage is said: Every change says each new figure once it stops changing; Thresholds, only once it passes a multiple of the damage interval." Opponents' damage the same about "everyone else's damage ..., teammates included". Damage interval: "The spacing of the damage thresholds: with Thresholds, damage is said once it passes a multiple of this." The Match announcements switch's description is unchanged; issues 03 and 04 may want it to grow.
- **Tests.** `match_announcer` gains Thresholds for each side (one crossing, a rise short of one, several at once said once, reaching a multiple), a heal's fall then a crossing (85, 55, 58 silent, 65 said), a KO, the interval and the mode changed partway through (to Every change, back, and Off and on again), a teammate following the opponents' row and not yours, and GAME! dropping only a figure that crossed; the rows test checks the three rows' place, kinds, options, defaults and range, each read live, a step Left from Off wrapping to Thresholds, and the file's text read back.
- **Drive scripts.** `launcher_a11y.drive`, `port_menu_a11y.drive` and `port_menu_voices.drive` read the three rows after the switch, their step counts past them changed; `launcher_a11y.drive` also steps Your damage to Thresholds and back, checking `match_announcements.your_damage = Thresholds` in `a11y.ini`. The `verify` skill has the unit test's new cases, the scripts' rows and the Thresholds run of the debug match.
- **Verification**, quick and menus levels with `MELEE_A11Y_CUES=1`: build with the "Rollback sections" line, style, unit tests 34 of 35 (`launcher_data`, known), `run_level.py menus` and the seven match scripts 35 of 35. The debug match against a CPU with both sides on Thresholds (a scratch settings file) said `voice yours: "21"`, `"46"`, `"60"` and `voice opponents: "20"`, `"41"`, each with its `plays` line and nothing between; with 12 jabs Mario's damage crossed nothing and nothing was said, as it should.
- **Left open.** The play test: whether 20 is a good default interval, and the wording of the rows.
