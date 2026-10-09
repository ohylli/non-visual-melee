# 01 Stage names, the opening and hover speech

Status: ready-for-agent
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
