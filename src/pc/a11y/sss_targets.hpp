/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage select's targets: the stage tiles in the rows a player sees, where a
 * D-pad step goes among them, where a glide aims, and when the D-pad steers
 * at all (.scratch/stage-select/spec.md, "Steering" and "Steps"). A target
 * is a tile, by its index in the game's table. The game keeps the tiles'
 * places in their models only, and they slide in as the screen opens, so
 * the rows are built afresh from each snapshot. Pure: it touches no game
 * state. */
#pragma once
#include "game_access.h"
#include "steering.hpp"
#include "steps.hpp"

namespace a11y {

/* The drawn tiles, locked ones included, in rows top to bottom, each left to
 * right, anchored at their centres. Tiles whose centres are level make a
 * row; a tile tall enough to reach into two rows (Icicle Mountain, Flat
 * Zone) is in both. Tiles not drawn are in none, so a row of them does not
 * exist. */
PlacedRows<int> tile_rows(const A11ySssState& state);

/* The tile the stage cursor at `at` is on, as fn_8025A310 tests it: the
 * first drawn tile in the table's order whose hit box holds it.
 * A11Y_SSS_NO_TILE over empty space. */
int tile_at(const A11ySssState& state, Point at);

/* Where a glide to the tile aims: its hit box's centre. */
Point aim_point(const A11ySssState& state, int tile);

/* Where a step goes. */
struct TileStep {
    /* The tile to name: the destination, or at an edge the tile the cursor
     * is on or going to; with nothing that way from empty space, the tile
     * A would pick there. A11Y_SSS_NO_TILE for none. */
    int tile = A11Y_SSS_NO_TILE;
    /* The cursor goes there; false at an edge. */
    bool moves = false;
    /* The row the step was taken along, which Left and Right from a tall
     * tile go back along; -1 for none. */
    int row = -1;
};

/* A step from `from`, the tile the cursor is going to or on
 * (A11Y_SSS_NO_TILE over empty space), with the cursor at `at`. in_row is
 * the row the step that reached `from` was taken along, or -1. While no
 * tile has been hovered since the screen opened, any direction goes to the
 * first tile of the top row. Then along from's row, Up and Down to the tile
 * nearest in x in the next row that has one, no wrapping; from empty space,
 * the nearest tile that way. */
TileStep step(const A11ySssState& state, int from, Point at, Direction direction, int in_row);

/* The D-pad steers the stage cursor: no pick made yet, and the cursor
 * answers to controller 1, to every controller, or online to the local
 * player. Steering writes controller 1's pad only. */
bool may_steer(const A11ySssState& state);

/* The game's port whose pad steering's presses arrive on: controller 1's,
 * which online is the local player's. */
int steering_port(const A11ySssState& state);

/* The stick as the pad held it, from the stick that moved the stage cursor,
 * which has the dead zone of 30 taken off each axis beyond it. An axis
 * within the dead zone arrives as 0 and reads as at rest. */
Stick stick_as_held(int x, int y);

}  // namespace a11y
