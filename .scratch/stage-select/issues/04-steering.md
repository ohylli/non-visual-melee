# 04 Steering on stage select

Status: resolved (2026-10-09)
Type: task
Blocked by: 01, 03

Spec: `.scratch/stage-select/spec.md`, "Steering" and "Steps".

## Machinery

- **Stage cursor hook** in `fn_8025A310` after the clamp: the cursor's position and the stick the game applied this frame. Offline the stick is merged, so it may be another port's. One line, declared in `a11y_hooks.h`. The glide advances here, once per simulated frame.
- **Targets** in a new stage select targets file on issue 03's shared step algorithm:
  - rows built from the tiles' positions, with undrawn tiles absent and locked tiles present;
  - Icicle Mountain and Flat Zone in rows 1 and 2, remembering which row the player came from;
  - the first press while nothing has been hovered goes to Icicle Mountain.
- **Where steering is off**: as the spec lists.

## Behaviour

- Every direction names the destination at the press; an edge repeats the current tile.
- Hovers crossed during a glide go unsaid and arrival is silent. A glide ending elsewhere names where it is, and a failed one says "Could not reach Corneria".
- The player's stick abandons the glide.
- A pick landing on a different stage from the last one spoken: "Picked Fountain of Dreams."
- Under the Turns or Loser rule with another player's cursor: no steering.

## Done when

- Unit tests cover:
  - steps from every tile in every direction, with all stages unlocked and with a fresh save's locks;
  - the tall tiles from both rows;
  - the first press in each direction;
  - steps from empty space;
  - the mid-glide pick;
  - no steering for another port.
- A drive script under `tools/a11y/` steps into the grid, walks rows and columns waiting on each name, steps to Random and picks it, and checks the glide lines. A second picks with A during a glide and waits on "Picked ...".
- A run with steering on, left on stage select with no input, shows the stick at rest.
- The `verify` skill lists the scripts; `python tools/check_style.py` passes; the footprint grep shows the hook.
- Docs:
  - ADR-0004's consequence about stage select's formula is updated;
  - `docs/a11y/native-menus.md` records the movement rule;
  - the CLAUDE.md status line mentions steering.

## Comments

### 2026-10-09, implementation (agent)

Implemented; hearing it is issue 07.

- **Stage cursor hook.** `pc_a11y_sss_cursor(x, y, stick_x, stick_y)` in `fn_8025A310`, after the clamp and the cursor's world position is read, before the tiles are tested: one line, declared in `a11y_hooks.h`. It passes the stick as the function sees it, with the dead zone taken off; `stick_as_held` puts the 30 back on each nonzero axis for the glide, as issue 03 suggested (an axis within the dead zone arrives as 0 and reads as at rest, which is all the glide needs). The function returns early once a pick is made, so the hook falls silent there; the frame hook stops a glide under way as "steering is off here".
- **The real start and units.** A run logged the cursor at (0, -13) on arrival, below every tile, and its world position moving exactly 0.03 per stick unit beyond the dead zone, so the model's translation and the tiles' test share units. `test_steering`'s stage cursor now starts there; its longest glide is unchanged (54 frames from a corner). `native-menus.md` records the start, the dead zone taken off before the cursor's update, and the merged cursor's port order.
- **Targets** (`sss_targets.cpp`, pure): rows built afresh from each snapshot. Drawn tiles whose centres are within 0.5 in height make a band; a band whose boxes reach at least 1.0 into two other bands is of tall tiles, which join both and make no row of their own (Icicle Mountain and Flat Zone reach 2.0 and 1.7 into the second row; ordinary rows have gaps). Locked tiles are targets, undrawn ones are not, so a fresh save has four rows. `tile_at` repeats the game's hit test; the aim is a tile's centre, retargeted each frame, so a glide started while the tiles still slide in follows its tile. `step` adds the first-press rule (any direction to the first tile of the top row while nothing has been hovered) and returns the row a step was taken along; the reader keeps it with the tile it named, so Right from Icicle Mountain returns to the row the player came from, row 1 when they never left it. From empty space with nothing that way, the step names the tile A would pick there (the last hovered) and does not move: the spec's "an edge repeats the current tile" read for empty space.
- **Steering off**: `may_steer` is false once a pick is made, for the skipped scene, and offline when the cursor answers to a port other than controller 1 (Turns, Loser). Presses are read from controller 1's pad offline and from `pc_net_local_player()`'s online (`steering_port`), as character select does. Online steering is therefore on, but unverified: issue 05 (open question 1).
- **Speech.** `SssSpeech::step` says `hover_words` at the press, the description included; during a glide and on the frame it ends, hovers go unsaid; a glide ending elsewhere names the hovered tile; a failed one says "Could not reach Corneria" (`tile_words`) and, if elsewhere, where it is. A named pick on another tile than the one last named by a hover or a step says "Picked Rainbow Cruise."; offline only, online picks staying issue 05's. A Random roll during a glide says "Random: ..." as before.
- **Tests.** `test_sss_targets` (new): the rows with and without a fresh save's locks, every direction from every tile in both (tables of 30 and 24 tiles), both tall tiles from both rows and an edge keeping the row, the first press in every direction, steps from empty space, the hit test, `may_steer` for another port, online and after a pick, and `stick_as_held`. `test_sss_speech`: the step at the press, silent crossing and arrival, a glide ending elsewhere, a failed glide, the mid-glide pick, a pick where the step went, and a Random roll during a glide.
- **Drive.** `tools/a11y/sss_steps.drive`: the first press (Down) to Icicle Mountain, along the top row, down to Jungle Japes, left to Icicle Mountain and Right back to Rainbow Cruise (row memory), down through Random, Brinstar and Brinstar Depths to Battlefield, Down at the edge, up and along to Random, then A ("Random: Brinstar."). `tools/a11y/sss_glide_pick.drive`: drive.py's presses follow each other about 8 frames apart, longer than a step between neighbours (5 to 8 frames), so a pick during an ordinary step glide cannot be timed. The script instead takes the cursor from Rainbow Cruise to the bottom left corner with the stick (crossing no tile, silent), steps Up to Battlefield from there (about 19 frames, under empty space until the last ones) and presses A at once: the glide stops after 8 frames and "Picked Rainbow Cruise." is said, then the match on Rainbow Cruise. Passed twice.
- **Stick at rest.** A drive run left on stage select for 10 seconds with steering on and `MELEE_INPUT_TRACE=1` (drive.py sets both) logged one pad line after the opening, the Start release with stick 0,0, and nothing more.
- **Glide feel (spec open question 3), as far as the log shows.** Steps between neighbours glide in 5 to 8 frames, the first press from the start to Icicle Mountain 33 frames (about half a second, diagonal at 0.78 per axis per frame because the clamp scales a diagonal push back), the corner to Battlefield about 19, and the longest in `test_steering` 54 frames. All under a second; whether that feels right is for the play test.
- **Docs.** ADR-0004: its opening names stage select as steered, and a consequence covers the merged cursor (another controller's stick stops a glide); the movement rule consequence was already updated by issue 03. `native-menus.md` as above, plus the second hook. CLAUDE.md's stage select line names steering, the new files and both scripts. The `verify` skill lists the two scripts (thirteen menu scripts).
- **Verification**, online level: build, style, unit tests (`sss_targets` new, `sss_speech` and `steering` updated; `launcher_data` fails as known), a bounded `title` run, all thirteen menu drive scripts (`sss_steps.drive` lost its presses on the title screen once and passed on an unchanged rerun), the five `css_net.drive` runs (recordings agree, through stage select) and the `lan_lobby.drive` run passed.

For issue 05: online steering on stage select is enabled by the same rule as character select (presses from the local player's pad, which online is controller 1) but no run has stepped there; "Picked ..." and every pick word are offline only; online the glide stops at the pick like offline.

