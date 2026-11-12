# 03 The item snapshot and the container names

Status: ready-for-agent
Type: task
Blocked by: none

Spec: `.scratch/items/spec.md` ("What the game gives us", "Which items", "The snapshot", open questions 1, 2, 3 and 5). Read it first, then `docs/a11y/gameplay-state.md` ("Reading safely", "Items") and `docs/a11y/cues.md` ("The match snapshot").

## What to build

- **The items in the match snapshot.** `a11y_game_match_state` (`game_access.c`) walks the item list once a tick and keeps a short list of items in the fork's sense: of a common kind and not made by a fighter (`flags.x1`). Each holds its serial (`x1C`), kind, position, whether it is loose, whether it is within reach of you, and who holds it (a slot, or none). The list is ordered nearest to you first, by straight-line distance, and its length is a constant, long enough to find a Party Ball's whole burst. The snapshot also says how many items there were, so a reader knows when the list was cut short.
- **Loose**: grabbable (`Item_IsGrabbable`), not held, and not flying after a throw; one thrown is loose again once it lands (open question 2 settles the field).
- **Within reach**: `ftpickupitem_800942A0(your fighter, 3)` returns the item; mark that one. Called from the snapshot only, between ticks, as the floor cast is; it only reads (the spec, "Within reach"). Not called while you are out.
- **Taking, for issue 06.** Per slot, what the spec's open question 1 finds is needed to see a take: the items in hand (`item_gobj`, `x1978`, `x197C`) by serial, and whatever marks a touch item taken and by whom. Find the answer by probing and put it in the snapshot.
- **Names.** `item_kinds.h` and `item_names.cpp` gain Capsule, Crate, Barrel and Egg (`It_Kind_Capsule`, `It_Kind_Box`, `It_Kind_Taru`, `It_Kind_Egg` in `forward.h`), keeping the static asserts whole; Item Switch and the Training menu are unaffected.
- **The open questions.** Answer 1, 2, 3 and 5 by probing in Training (its item menu brings any item in beside you; `tools/a11y/training_menu.drive` reaches it) and in a VS match with items on if one can be had (question 4 is issue 07's, but note what you learn). Temporary logging is fine; remove it before committing.

## Done when

- A unit test of whatever pure logic the snapshot hands to C++ (ordering, loose) if any sits outside `game_access.c`; the container names in the item names' test.
- A drive run in Training, with temporary logging, shows an item spawned from the menu entering the snapshot with its serial and kind, loose, within reach when you stand at it facing it and not when facing away, and leaving the list or changing holder when you pick it up; the logging removed afterwards.
- `docs/a11y/gameplay-state.md`, "Items", has what the spec found that it lacks (taking by touch, the tally, the within-reach box, made by a fighter) and the answers to questions 1, 2, 3 and 5, recorded here in a comment too.
- `cmake --build build` passes, as do the unit tests and `python tools/check_style.py`; the `verify` skill's quick level with `MELEE_A11Y_CUES=1`, as the snapshot changed.

## Comments
