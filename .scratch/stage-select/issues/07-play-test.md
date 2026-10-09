# 07 Play test by ear

Status: ready-for-human
Type: task
Blocked by: 04, 05, 06

The maintainer plays stage select with NVDA, offline and online against a friend or a second instance.

Listen for:

- The opening, and whether "Start picks a random stage" is understood.
- Steps: rows, the tall tiles, the first press going to Icicle Mountain, edges, locked tiles.
- Glides: speed, and whether the game's move sound for each crossed tile helps or is a nuisance.
- Picks: Random's roll, "Locked.", "Picked ..." after an early A.
- Online: waiting and the result wording.
- Descriptions (`sss_names.cpp`), said after the name on every hover: length, usefulness, whether they slow browsing, and any that mislead. They were written from the small tile pictures and the stages' code, not from play, so a player who knows the stages should check what each says about the ground's shape and the hazards.
- The Turns or Loser rule with another player choosing.

Findings become new issues here. When it passes, the CLAUDE.md status line says "Play-tested by ear with NVDA".

## Comments

### Code review of 01 to 06, 2026-10-09

Left for the maintainer, most severe first; each worth listening for in the play test.

- Start after browsing: the opening says "Start picks a random stage", but once the stage cursor has entered any tile, Start (like A) picks the last tile entered, and a named pick is silent, so a player pressing Start "for random" gets the last stage heard without being told. Options: reword the opening ("D-pad to choose, A to pick, or Start now for a random stage."), or name every pick. Recommended: reword the opening.
- Ranked online's stage list is now silent: its scene announcement ("Stage select. No speech yet.") was removed with the grid's opening, and the frame hook never runs on the list. Recommended: say "Ranked stage list. No speech yet." at scene entry while `pc_rank_session_active()`, until the list gets its own speech.
- A press at an edge repeats the tile's whole description. Recommended: the name alone at an edge.
- From empty space with no tile that way, a step names the tile last hovered as if the cursor were on it. Option: "Nothing that way." with what A would pick.
- Design: `CssReader::steer` and `SssReader::cursor` repeat one stop, pad, press and glide frame sequence, and `hooks.cpp` switches twice on the scene for the steering reader. A per-frame method on `Steerer` and one lookup of the screen's reader would remove both; worth it when a third screen steers.
