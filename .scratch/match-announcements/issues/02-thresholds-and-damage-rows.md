# 02 Threshold mode and the damage rows

Status: ready-for-agent
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
