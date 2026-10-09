/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A hand-written snapshot of stage select for the tests of its reader. The
 * tiles' positions and half extents are those a run reported once the tiles
 * had slid into place (on the screen's first frames they are still on their
 * way in from the right). */
#pragma once
#include "game_access.h"
#include "stage_kinds.h"

namespace sss_test {

/* Tiles by their index in the game's table. */
constexpr int kPeachsCastleTile = 0;
constexpr int kTempleTile = 5;
constexpr int kFountainTile = 8;
constexpr int kCorneriaTile = 10;
constexpr int kBrinstarTile = 12;
constexpr int kBrinstarDepthsTile = 13;
constexpr int kOnettTile = 14;
constexpr int kFoursideTile = 15;
constexpr int kBattlefieldTile = 24;

struct TileRow {
    A11yStage stage;
    float x;
    float y;
    float half_width;
    float half_height;
};

/* Every tile in the table's order: the stacked pairs (upper, lower), Icicle
 * Mountain, Flat Zone, the bottom row, the random tile. */
constexpr TileRow kTiles[A11Y_SSS_TILES] = {
    {A11Y_St_Kind_Castle, -16.498f, 15.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_RCruise, -16.498f, 10.1f, 3.1f, 2.7f},
    {A11Y_St_Kind_Kongo, -9.898f, 15.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Garden, -9.898f, 10.1f, 3.1f, 2.7f},
    {A11Y_St_Kind_GreatBay, -3.299f, 15.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Shrine, -3.299f, 10.1f, 3.1f, 2.7f},
    {A11Y_St_Kind_Story, 3.3f, 15.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Yoster, 3.3f, 10.1f, 2.1f, 2.7f},
    {A11Y_St_Kind_Izumi, 9.899f, 15.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Greens, 9.899f, 10.1f, 3.1f, 2.7f},
    {A11Y_St_Kind_Corneria, 16.499f, 15.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Venom, 16.499f, 10.1f, 3.1f, 2.7f},
    {A11Y_St_Kind_Zebes, -4.6f, 3.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Kraid, -4.6f, -1.9f, 3.1f, 2.7f},
    {A11Y_St_Kind_Onett, 2.0f, 3.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Fourside, 2.0f, -1.9f, 3.1f, 2.7f},
    {A11Y_St_Kind_MuteCity, 8.6f, 3.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_BigBlue, 8.6f, -1.9f, 3.1f, 2.7f},
    {A11Y_St_Kind_PStadium, 15.199f, 3.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Pura, 15.199f, -1.9f, 3.1f, 2.7f},
    {A11Y_St_Kind_Inishie1, 21.8f, 3.7f, 3.1f, 2.7f},
    {A11Y_St_Kind_Inishie2, 21.8f, -1.9f, 3.1f, 2.7f},
    {A11Y_St_Kind_Icemt, -23.1f, 13.7f, 3.1f, 2.9f},
    {A11Y_St_Kind_Flatzone, 23.1f, 14.0f, 3.1f, 2.9f},
    {A11Y_St_Kind_Battle, 1.3f, -9.1f, 2.9f, 2.1f},
    {A11Y_St_Kind_Last, 6.6f, -9.1f, 2.9f, 2.1f},
    {A11Y_St_Kind_OldPupupu, 12.3f, -9.1f, 2.9f, 2.1f},
    {A11Y_St_Kind_OldYoshi, 17.6f, -9.1f, 2.9f, 2.1f},
    {A11Y_St_Kind_OldKongo, 22.899f, -9.1f, 2.9f, 2.1f},
    {A11Y_NO_STAGE, -14.1f, 3.6f, 3.6f, 2.7f},
};

/* Stage select as it opens offline with every stage unlocked, the stage
 * cursor answering to every controller and on no tile yet. */
inline A11ySssState screen() {
    A11ySssState state{};
    state.owner = -1;
    state.hovered = A11Y_SSS_NO_TILE;
    state.pick = A11Y_SSS_CHOOSING;
    state.local_pick = -1;
    state.remote_pick = -1;
    for (int i = 0; i < A11Y_SSS_TILES; i++) {
        const TileRow& row = kTiles[i];
        state.tiles[i] = A11ySssTile{
            row.stage, A11Y_SSS_UNLOCKED, row.x, row.y, row.half_width, row.half_height};
    }
    return state;
}

/* The same with the stages docs/screenshots/stage-select.png shows locked:
 * the five under the block beside the random tile drawn as "?", Flat Zone
 * and the bottom row not drawn at all. */
inline A11ySssState screen_with_locked_stages() {
    A11ySssState state = screen();
    for (int tile : {13, 15, 17, 19, 21}) {
        state.tiles[tile].state = A11Y_SSS_LOCKED;
    }
    for (int tile = 23; tile < A11Y_SSS_RANDOM_TILE; tile++) {
        state.tiles[tile] = A11ySssTile{state.tiles[tile].stage, A11Y_SSS_NOT_DRAWN, 0.0f, 0.0f,
            state.tiles[tile].half_width, state.tiles[tile].half_height};
    }
    return state;
}

}  // namespace sss_test
