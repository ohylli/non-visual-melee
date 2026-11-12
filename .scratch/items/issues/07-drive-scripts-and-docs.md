# 07 Drive scripts, the online run and the docs

Status: ready-for-agent
Type: task
Blocked by: 04, 05, 06

Spec: `.scratch/items/spec.md` ("Tests", "Docs", open question 4).

## What to build

- **A VS match with items.** Answer open question 4: how a debug match (`MELEE_DEBUG_VS`) can run with items on at a high frequency, or else a drive script that turns the item switch on through the rules screens. Then a drive script of a match against a CPU with items on at Very High, long enough for item switch spawns, a container breaking, and a CPU picking something up.
- **Training's script** for the appearance, the item's position cue at within reach, and your pickup, kept from issues 04 to 06 as a script of its own.
- **The online run.** Through `tools/a11y/drive_net.py`, fork against fork and against the base port (`--base-b`), with items on if the online rules allow it (Ranked forces them off): appearances and pickups said on both sides, the session staying in sync.
- **The `verify` skill**: the scripts, the expected lines, the cue dump checks and any known failure.
- **Docs**: `docs/a11y/cues.md` sections on the appearance cue and the item's position cue, beside the other match cues; `docs/a11y/gameplay-state.md` checked for what issues 03 to 06 learned; `ACCESSIBILITY.md` with what a player hears of items, the four new rows and the sounds; one "Accessibility status" line in `CLAUDE.md`, naming the play test as waiting with issue 08's path; whether `resources/a11y/` holds a note naming the sounds' sources (open question 6), as the maintainer decides.

## Done when

- The scripts pass in `run_level.py` at the level the `verify` skill puts them, and the online run stays in sync against both peers.
- The docs and the status line are in.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`.

## Comments
