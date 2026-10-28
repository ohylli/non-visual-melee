# 05 Drive scripts, the online run, the verify skill, the docs and the status line

Status: ready-for-agent
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
