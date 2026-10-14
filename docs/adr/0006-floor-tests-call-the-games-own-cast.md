# Floor tests call the game's own cast from the tick hook

Status: accepted (2026-10-14)

The gap warning asks, every tick you are in the air, whether any floor lies straight below you; the ledge warning asks the same just past each end of the floor below you. The game answers that question with `mpCheckFloor` (`src/melee/mp/mplib.c`), a raycast against every floor line, platforms included, which the CPU players also use to judge their recoveries. The fork calls it from `pc_a11y_match_tick`, the tick hook in `gm_RunSimTick`, after every fighter has moved for the tick.

The call is not a pure read. To skip far-away parts of the stage it marks joints (`CollJoint_TooFar`) and sets the flag `didCheckBounding`, and clears both before returning, but only when the flag was clear on entry. Every user of the marks in the game pairs them the same way, and the hook runs outside the fighters' update, where the flag is clear and no joint is marked. So the call leaves the collision data exactly as it found it: nothing the simulation, a rollback snapshot or the peer's game reads changes, and online play stays in step with a base port peer. Everything else it calls only reads. The trace is in `docs/a11y/gameplay-state.md`, "Is there ground below?".

## Considered options

- **A line search in fork code**: walk the stage's line arrays and intersect them in fork code, writing nothing at all. But it must copy what the game's cast does around the lines: the joined-end extension (`mpLib_8004ED5C`, a floor's end reaching a unit further where another line joins it), the joints' enabled and hidden states, and moving stage parts. Any difference makes the fork's answer disagree with where the game lands the fighter, for no gain in safety once the cast is shown to leave no trace.
- **Calling the cast inside the fighters' update**, at the moment a fighter's own collision runs: there the marks for that fighter's step are already set, so the cast reuses them, can skip a floor and wrongly answer "nothing below". Rejected.
- **No cast, only the fighter's own collision result**: `coll_data.floor.index` names the floor under a fighter on the ground, but nothing tells an airborne fighter what lies below, which is the whole question.

## Consequences

- CLAUDE.md's rule that the fork changes nothing but the pad hook's input stands, with this one reading: a call that writes only temporary state it clears before returning, and finds that state clear, counts as a read.
- Casts belong in the tick hook only. A cast from a hook inside the fighters' update would be wrong and is not allowed. The same holds for `mpCheckLeftWall`, `mpCheckRightWall` and `mpCheckMultiple`, should a later cue need them.
- The cast skips the platform a fighter is dropping through (`coll_data.floor_skip`), as the game's own collision step does, so the fork's answer matches the game's.
- A decomp sync that changes `mpCheckFloor` or the joint marks needs the trace redone before the claim above is trusted again.
