# 04 Drive scripts, verify skill and status line

Status: ready-for-agent
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
