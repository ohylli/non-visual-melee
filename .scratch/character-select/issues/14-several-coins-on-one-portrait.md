# 14 Several coins on one portrait: only one is said

Status: resolved (2026-11-02)
Type: task
Blocked by:

Found in the play test of issue 08. When more than one coin rests on the same character, reaching that portrait names only one of the coins.

Details to settle when this is picked up.

## Comments

2026-11-02: each coin the free hand may pick up (its own and CPUs') is now a target of its own in the portrait rows (`Target::coin`, `target_rows` in `css_targets.cpp`), the player's own first, then by slot. Left and Right step through a portrait's coins before the next portrait; Up and Down land on its first. The first step onto a portrait names every coin resting there, the one the hand goes to first ("Pichu, the CPU's coin, also your coin"); a step from coin to coin on one portrait names only the coin ("your coin"). Another human's coin is named but is no step, since the game lets only its owner pick it up. Each coin's glide aims where A picks up that coin and no other: the game takes the nearest coin within reach and pushes coins resting together apart, so they never stay stacked. `css_coins.drive` checks both coins picked up where their steps named them.

2026-11-02: the maintainer's play test, with three and four coins on Peach, found two more faults, both fixed. A coin the game crowded to its portrait's left edge is picked up from a point that hovers the portrait to the left, so steps went from the wrong portrait: `locate` now puts the hand on the coin A would pick up there, whatever portrait it is over. A coin crowded to the top put its aim a unit below the top bar, and a push the game applied once more than the glide counted carried the hand onto the rules header: the aim keeps further from the top bar, and every glide now watches the cursor a few frames after reaching its destination and steers back should it move (`kSettleFrames` in `steering.cpp`, ADR-0004). Play-tested by ear after both fixes.
