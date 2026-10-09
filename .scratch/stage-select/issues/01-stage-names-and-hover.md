# 01 Stage names, the opening and hover speech

Status: resolved (2026-10-09)
Type: task

The first slice of `.scratch/stage-select/spec.md`: stage select speaks, before any steering. Read the spec's "What the game gives us" and "Speech" first.

## Machinery

- **Per-frame hook** at the top of `mnStageSel_Scene_OnFrame`, after the ranked branch and before the early returns: the cell table, the hovered index (`mnStageSel_804D6CAE`), the pick state (`804D6CAF`, `804D6CA4`), the owning port (`804D50A0`). One line in the base port file, declared in `a11y_hooks.h`.
- **Snapshot** filled in C (`game_access.c`), as `a11y_game_css_state` does: per tile its kind, drawn, locked or unlocked, world position and half extents; the hovered index; picked or not; the owning port.
- **Reader and speech** in new fork files following `css_reader` and `css_speech`; `forget()` on scene entry.
- **Stage name table**: a C kinds header for `StKind` checked against the decomp's numbers (as `character_kinds.h`), and `stage_name(kind)` with the names the spec lists.
- **Scene announcement**: `scene_speech.cpp` stops saying stage select's words offline and online; the reader speaks its own opening.

## Behaviour

- The opening: "Stage select. D-pad to choose, Start picks a random stage." When the stage cursor belongs to a port other than controller 1 (Turns or Loser rule), it adds "Player 3 chooses. Steering needs controller 1."
- Hover: each change of the hovered index says the stage's name, "Random" or "Locked stage". Nothing on leaving a tile into empty space.
- Picks offline: Random says "Random: Corneria." once the roll is known; a refused pick on a locked tile says "Locked."; a named pick is silent.
- Silent while `pc_net_resim()` is true.

## Done when

- `cmake --build build` passes; unit tests cover the opening in both forms, hover of a stage, the random tile and a locked tile, the Random roll, the refusal, and silence on a named pick.
- A drive script under `tools/a11y/` reaches stage select from VS mode, moves the stage cursor with the stick onto tiles, checks the hover lines, picks Random and waits on "Random: ..." and the match.
- The `verify` skill lists the script; `python tools/check_style.py` passes; the footprint grep shows the new hook.
- `docs/a11y/native-menus.md` corrects "Start then picks a random stage": only until the cursor first enters a tile.
- CLAUDE.md gains the "Accessibility status" line for stage select.

## Comments

### 2026-10-09, implementation (agent)

Implemented; hearing it is issue 07.

- **Hook.** `pc_a11y_sss_frame` in `mnStageSel_Scene_OnFrame`, after the ranked branch and before the skipped scene's early return, one line. It passes the screen's data, the tile table with the size of its row, the hovered index, the pick state, the input freeze, the buttons the screen read, the owning port, and already the two online picks (`net_local_pick`, `net_remote_pick`) for issue 05, so that issue need not touch the line again. The table's row type is private to `mnstagesel.c`, so `game_access.c` keeps a copy of its layout and checks the passed size at run time; a mismatch after a decomp sync logs one line and keeps stage select silent rather than read the wrong fields.
- **Snapshot.** `a11y_game_sss_state` fills per tile its stage (`stage_kinds.h`, checked against `StKind` at compile time), drawn, locked or unlocked, the hit box's centre (read with `lb_8000B1CC`, as the cursor's own test does) and half extents; the hovered index, the pick state, the owner, whether A or Start reached the pick, and the online picks.
- **Order within a frame**, the key to picks: `OnFrame` runs first (reads the buttons, counts the freeze down), then the objects: the name object (proc priority 1), which runs the pick, before the cursor (priority 2), which moves and hit-tests. So the buttons a snapshot sees acted on the tile the previous snapshot saw hovered, and the freeze at the hook is the value the pick saw. Random and the refusal use the previous snapshot's hovered index.
- **Speech** (`sss_speech.cpp`, `sss_reader.cpp`, `sss_names.cpp`): the opening, "Player 3 chooses. Steering needs controller 1." offline when the owner is another port (online the cursor is always the local player's, so never there), hover with the stage, "Random" or "Locked stage", "Random: Corneria." offline, "Locked." on a refused A or Start, a named pick silent. Picks online say nothing yet (issue 05). The scene table no longer speaks stage select, offline or online.
- **Stage names, checked against screenshots.** The game draws a two-line picture: a small line over the name in large letters. The small line is the stage's world ("Mushroom Kingdom" over "Princess Peach's Castle", "Eagleland" over "Onett", "Past Stages" over "Dream Land"), except where the two read as one name: "Hyrule" over "Temple" is spoken "Hyrule Temple", "Mushroom" over "Kingdom" (and "Kingdom II") "Mushroom Kingdom". The past stages take the spec's "N64" names.
- **Tiles slide in.** On the screen's first frames every tile is still on its way in from the right (x 36 on the first frame); positions settle within a couple of seconds. Issue 04 must build its rows from settled positions, or rebuild them, not from the first snapshot. The settled geometry is in `test_sss_screen.hpp`. Yoshi's Island's hit box is narrower than the others (half width 2.1, not 3.1), and the stacked columns leave gaps of 0.4 between boxes, so the cursor can rest between two tiles hovering neither; aim at a tile's centre.
- **The skipped scene (open question 2).** The hook runs on its one frame too, and `sss_data->force_stage_id` already holds the forced stage's `StKind` there; the snapshot reports it as skipped with that stage and reads nothing else (the table still points at models of an earlier stage select, freed). The reader stays silent; issue 02 speaks it from the snapshot without a new hook. Until then the skipped scene says nothing, where it used to say the scene table's stage select words.
- **Online until issue 05.** The opening is the same online; the old "then wait for your opponent" is gone until 05's "Waiting for your opponent." `css_net.drive` waits on the new opening.
- **Ranked online stage select** now arrives silent: its list runs before the hook, and the scene table no longer speaks stage select (its old words, about Start and a random stage, were wrong for the list anyway). Left for the ranked follow-up the spec notes.
- The opening says "D-pad to choose" though steering arrives with issue 04, as the spec words it.
- **Drive.** `tools/a11y/sss_hover.drive` reaches stage select from VS mode (Fox and a CPU by D-pad), moves the stage cursor up through Battlefield, Fourside and Onett, left through Brinstar to Random, and A says "Random: ...", then the match. It passed twice ("Random: Brinstar Depths."). Locked tiles, the refusal and the Turns and Loser opening are out of the drivers' reach (`--no-card` unlocks everything; the rules need setting) and rest on the unit tests.
- **Verification**, online level: build, style, unit tests (`sss_speech` new, `scene_speech` updated; `launcher_data` fails as known), a bounded `title` run, all ten menu drive scripts, the five `css_net.drive` runs and the `lan_lobby.drive` run passed.
