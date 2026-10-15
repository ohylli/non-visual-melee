# 01 The Training menu reader

Status: ready-for-agent
Type: task

The reader of `.scratch/training-menu/spec.md`: everything but the drive script. Read the spec first, then `docs/a11y/native-menus.md` and `docs/a11y/gameplay-state.md` ("The tick hook", "Pause, freezes and slow motion").

## Machinery

- **Snapshot.** `game_access.c` reads `gm_80473814`: the row (`x00`), whether the menu is open, and the hovered row's value from `menu_values`. Find what marks the menu open (spec, open questions 1 and 2).
- **Tick hook.** `pc_a11y_match_tick` in `hooks.cpp` feeds the reader each tick, whether or not the cue system exists; the cue system's tick stays as it is.
- **Session.** The hint is said on the first opening after each entry to the Training scene (spec, open question 3).
- **Names.** Row names and value words as the spec has them. Item names shared with Item Switch, keyed by the item (spec, open question 4); Item Switch's speech must not change.
- **Held directions.** The quarter-second rule on rows and values, on a clock the test can fake.

## Done when

- `cmake --build build` passes, and the unit tests pass with the reader's test covering the spec's list ("Tests").
- `test_menu_speech` still passes unchanged for Item Switch's names.
- A bounded run shows the match cues still start and stop as before.
- `python tools/check_style.py` passes.
- `docs/a11y/native-menus.md`'s Training menu entry corrected and extended as the spec's "Docs" says, with the answers to the spec's open questions where they belong.
