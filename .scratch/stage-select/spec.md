# Stage select: speech and steering

Status: agreed 2026-10-09 (design conversation with the maintainer)

The maintainer chose to steer stage select like character select (ADR-0004), to keep locked tiles as targets so nothing is hidden, the first step's destination, the online wording, and stage descriptions as a follow-up said with every hover. The wording of announcements is a first proposal for the play test to judge.

## Goal

A blind player can choose a stage alone, offline and online: hear which stage tile the stage cursor is on, step between tiles with the D-pad, pick a stage or Random, and know which stage the match will be on. Play stays compatible with base port builds online, and a sighted helper can still use the stick and be heard doing it.

How the game's menus run is in `docs/a11y/native-menus.md`; steering is ADR-0004 and the character select spec (`.scratch/character-select/spec.md`), whose rules this one reuses unless it says otherwise. Vocabulary is `CONTEXT.md`: stage cursor, stage tile, random tile, locked tile, steering, step, glide.

## Scope

In:

- **The normal grid** in every mode that uses stage select: VS, the Special Melee modes, Camera Mode, Training and Tournament, with the stage cursor merged over every controller.
- **One player's stage cursor**: the VS stage-select rules "Turns" and "Loser" hand the stage cursor to one player.
- **Online** (direct and LAN sessions): each player picks on their own, a coin flip decides.
- **The skipped scene**: with the stage-select rule Random or Ordered the game enters stage select and leaves on its first frame; the fork names the stage instead of announcing a screen.
- **Stage descriptions**, as their own issue.

Out, noted for later:

- Ranked online's stage list (`rankStageCreate`, `rankStageFrame`): a text list with bans, moved line by line with the stick or the D-pad, so it needs speech but no steering.
- Saying how many stages are not drawn at all. The maintainer chose to say nothing about them, as for a sighted player.

## What the game gives us

From reading `src/melee/mn/mnstagesel.c` and runs on 2026-10-09. Address-style names are the decomp's; their meaning here is inferred.

### State is private to one file

As on character select, every variable of the screen is `static`: the cell table, the hovered index, the pick state. The stage cursor's position lives only in its model's translation; no global points to it. The fork sees them through hook arguments.

### The layout

Five visual rows (an unlock-all screenshot from an online session, which forces unlock-all, and `docs/screenshots/stage-select.png` for a save with locked stages):

1. Icicle Mountain at the far left, Peach's Castle, Kongo Jungle, Great Bay, Yoshi's Story, Fountain of Dreams, Corneria, Flat Zone at the far right. Icicle Mountain and Flat Zone are tall tiles spanning rows 1 and 2.
2. Rainbow Cruise, Jungle Japes, Hyrule Temple, Yoshi's Island, Green Greens, Venom.
3. The random tile, then Brinstar, Onett, Mute City, Pokémon Stadium, Mushroom Kingdom, a block shifted right of the rows above.
4. Brinstar Depths, Fourside, Big Blue, Poké Floats, Mushroom Kingdom II: locked tiles until unlocked.
5. Battlefield, Final Destination, Dream Land N64, Yoshi's Island N64, Kongo Jungle N64: smaller tiles, not drawn while locked.

The stage cursor starts below every tile.

### The cell table

`mnStageSel_803F06D0[30]`: each row holds the tile's model (`x0`, positioned by `MnSlMap.dat`; read its world position with `lb_8000B1CC` once `mnStageSel_Scene_OnEnter` has run), `x8` (0 not drawn, 1 locked tile, 2 unlocked), `stkind`, and the hit box's half extents (`xC`, `x10`). Indices 0 to 21 are stacked pairs (even the upper tile, odd the lower), 22 Icicle Mountain, 23 Flat Zone, 24 to 28 the bottom row, 29 the random tile.

- `OnEnter` sets `x8` from `gm_80164430(stkind)`, which honours `pc_is_unlock_all_enabled()`. Locked tiles 22 to 28 become 0, not drawn; locked stacked tiles stay 1.
- Online sessions force unlock-all, so every tile is drawn there.

### Hover

`fn_8025A310` moves the stage cursor and hit-tests tiles 0 to 29 in order, skipping `x8 == 0`; the first box containing the cursor becomes `mnStageSel_804D6CAE`. Nothing resets it when the cursor leaves a tile, so the last tile stays hovered. 30 means nothing hovered yet. The game plays its move sound on each change.

### Movement

Per axis and linear, unlike character select's hand: a stick value beyond ±30 moves the cursor `0.03 * (s ∓ 30)` units a frame on that axis, at most 1.5 after the pad's radius-80 clamp. The cursor is clamped to x ±27, y ±19.

Whose stick moves it is `mnStageSel_804D50A0`:

- -1, merged: the first port (1 to 4) whose stick is past 30 on either axis drives it, and every port's buttons count. VS with stage select On, Training, Tournament.
- A port: that player alone (Turns and Loser rules, offline). The cursor wears that player's colour.
- Online: `pc_net_local_player()`.

### Input

A, B and Start, as presses. The D-pad, Z, L, R, X and Y do nothing on the grid, so steps reach the game harmlessly. L+R+Start soft-resets to the main menu where allowed. A, B and Start are ignored for the first 20 frames.

### Picking

`mnStageSel_80259C28`:

- Nothing hovered yet: Start picks Random; A does nothing.
- The random tile: A or Start picks Random. Offline the stage is rolled at once and the hovered index set to it; the game shows its name and preview.
- An unlocked tile: A or Start picks it. Over empty space this is the last tile hovered.
- A locked tile: the refused sound, nothing else.
- After a pick the cursor hides, input is frozen 30 frames, and the match loads once the preview has shown about 1.5 seconds (`mnStageSel_804D6CAF` 1, then 2).
- B, before a pick, returns to character select.

### Online

The local pick (a cell, 29 or 30 meaning Random) is sent (`netStageSel_SendPick`); `OnFrame` then waits while the opponent's pick is unknown. Nothing on screen shows the wait or the opponent's pick. Once both are known, `netStageSel_Resolve` flips a coin on the session's seed; the result lands in `mnStageSel_804D6CAE` and the match's rules, and the match loads at once. A lost peer ends the scene.

### The skipped scene

With the stage-select rule Random or Ordered, `gm_80167FC4` sets the stage beforehand and `OnFrame` leaves on its first frame. The scene announcement fires anyway today.

## Decisions

### Steering

As on character select (ADR-0004):

- A D-pad press (a step) names the destination at the press, and a glide takes the stage cursor there by generating stick input on port 1's virtual pad. The player presses A, B and Start themselves.
- Hovers crossed during a glide go unsaid; arrival where announced is silent.
- The player's own stick takes over at once; a later step glides from wherever the cursor is.
- A glide that fails says "Could not reach Corneria", logged.
- Every glide is logged with its destination and frames.
- Steering is off: outside stage select; once a pick is made; while the stage cursor belongs to a port other than controller 1; while `pc_net_resim()` is true; with the accessibility switch off (`MELEE_A11Y_STEER=1` turns it on alone for agent runs).

### Steps

- Rows are the visual rows, built at runtime from the tiles' positions, so rows not drawn do not exist for steps.
- Locked tiles are targets.
- Left and Right move along the row; Up and Down go to the tile nearest in x in the next row that has one. No wrapping: a press at an edge says the current tile again.
- Icicle Mountain and Flat Zone belong to rows 1 and 2. Left from Peach's Castle or Rainbow Cruise reaches Icicle Mountain; Right from it returns to the row the player came from, row 1 if they never left it. Flat Zone mirrors it. Down from Icicle Mountain reaches the random tile.
- First press: while nothing has been hovered since arrival, any direction goes to Icicle Mountain.
- From anywhere else (the player's stick left the cursor in empty space): character select's rule, Up and Down to the nearest row that way, Left and Right along the nearest row.

### Speech

- **Opening**, offline and online: "Stage select. D-pad to choose, Start picks a random stage." It replaces the scene announcement's "No speech yet" wording; the reader speaks it, as character select does.
- **Not controller 1's stage cursor**: the opening adds "Player 3 chooses. Steering needs controller 1." Hover speech still follows the cursor.
- **Hover**: the stage's name ("Corneria"), the random tile "Random", a locked tile "Locked stage". On a step at the press; by the stick, on entering a tile. Leaving a tile into empty space is silent.
- **Pick, offline**:
  - A named tile: silent; its name was just heard.
  - Random: "Random: Corneria."
  - A locked tile: "Locked."
  - A pick landing on a different stage from the last one spoken (A pressed during a glide): "Picked Fountain of Dreams."
- **Online**:
  - After the player's own pick: "Waiting for your opponent."
  - The result: "Corneria, your pick." or "Corneria, opponent's pick."; "Corneria." when both picked it; a Random pick that won says "Random: Corneria, your pick." (or opponent's).
  - The opponent's pick is never said before the result.
- **The skipped scene**: "Stage: Corneria." in place of the scene announcement.
- B returning to character select says nothing here; character select speaks its opening.

### Stage names and descriptions

- Names come from a fork string table keyed by `StKind`, with a C kinds header checked against the decomp's numbers like `character_kinds.h`. Names as the game draws them: "Pokémon Stadium", "Poké Floats", "Dream Land N64", "Yoshi's Island N64", "Kongo Jungle N64".
- Descriptions are one or two sentences each, written by the agent from screenshots of each stage's preview on stage select. They are said with the name on every hover, as one announcement: "Corneria. A fight on the hull of a ship flying over a city." The next step interrupts, so browsing stays quick. Locked tiles get no description.

### How the fork attaches

Hooks, one line each, declared in `a11y_hooks.h`:

- **Per frame**: at the top of `mnStageSel_Scene_OnFrame`, after the ranked branch and before the early returns. It passes the cell table, the hovered index, the pick state, the owning port and the online picks.
- **The stage cursor**: in `fn_8025A310` after the clamp, with the position and the stick the game applied, the input the glide needs. Offline the applied stick may come from another port.
- **The skipped scene** may need the stage from the match's rules at scene entry, or a check in the per-frame hook on its first frame.

### Shared code

- **The glide** (`steering.cpp`) takes the cursor's movement rule as a parameter: one for the hand, one for the stage cursor. Its delay, repeat and loss handling, takeover, give-up and wire rounding stay shared.
- **The step algorithm** moves out of `css_targets.cpp` into a generic one over rows of targets (along a row, nearest in x, no wrapping, from anywhere else); character select keeps its slider steps and coin-aware aim.
- **`pc_a11y_pad`** asks whichever screen is live for its stick; `pc_a11y_chat_buttons` also covers stage select, since quick chat uses the D-pad.
- **New files** by the character select pattern: a stage select reader, speech, targets and stage names. Coins, sliders, player slots and several hands do not carry over.

### Tests

- `test_steering` runs the glide against both movement rules.
- Stage select targets: every direction from every tile, both tall tiles from both rows, the first press, with and without locked and undrawn stages, from empty space.
- Stage select speech: hover, steps, glides, picks, the mid-glide pick, the not-controller-1 opening, online waiting and result.
- Drive scripts under `tools/a11y/` for offline steps and picks, and a `drive_net.py` run for online.

### Docs

- `docs/a11y/native-menus.md`: correct "Start then picks a random stage" (only until the cursor first enters a tile), and record the movement rule.
- CLAUDE.md: one "Accessibility status" line.
- ADR-0004: its consequence about stage select's formula is resolved by the parameter.
- The `verify` skill: new scripts and expected lines.

## Open questions

For the implementer to find out:

1. Whether online the local machine's controller 1 drives the stage cursor of `pc_net_local_player()` as it does the hand on character select.
2. Where the skipped scene's stage can be read at the moment the scene announcement would fire.
3. Whether glides across the grid feel right at 1.5 units a frame; the longest crosses about 50 units.

## Issues

1. `issues/01-stage-names-and-hover.md`: the per-frame hook, the stage name table, the opening, hover and offline pick speech.
2. `issues/02-skipped-scene.md`: "Stage: Corneria." for the Random and Ordered rules. Blocked by 01.
3. `issues/03-shared-steering.md`: the glide's movement rule as a parameter, the generic step algorithm, the pad dispatcher. Character select unchanged.
4. `issues/04-steering.md`: the stage cursor hook, steps over the tiles, glides, the mid-glide pick. Blocked by 01 and 03.
5. `issues/05-online.md`: waiting and result speech, two-instance runs against fork and base port. Blocked by 04.
6. `issues/06-stage-descriptions.md`: descriptions written from the previews, said with each hover. Blocked by 01.
7. `issues/07-play-test.md`: the maintainer, by ear. Blocked by 04, 05 and 06.

Docs are part of the issue they belong to.
