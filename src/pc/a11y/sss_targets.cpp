/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sss_targets.hpp"
#include <algorithm>
#include <optional>
#include <vector>

namespace a11y {
namespace {

/* Tiles whose centres are this close in height are in one row. */
constexpr float kLevel = 0.5f;
/* A row's boxes reach into another row's when they overlap it by this much
 * in height: the tall tiles reach 1.7 into the second row, and the rows of
 * ordinary tiles have gaps between them. */
constexpr float kReach = 1.0f;

bool drawn(const A11ySssState& state, int tile) {
    return tile >= 0 && tile < A11Y_SSS_TILES && state.tiles[tile].state != A11Y_SSS_NOT_DRAWN;
}

/* Tiles level with each other, and the height their boxes cover. */
struct Band {
    std::vector<int> tiles;
    float bottom = 0.0f;
    float top = 0.0f;
};

float overlap(const Band& a, const Band& b) {
    return std::min(a.top, b.top) - std::max(a.bottom, b.bottom);
}

/* A row holding both tiles, in_row first; -1 for none. */
int shared_row(const PlacedRows<int>& rows, int a, int b, int in) {
    if (in >= 0 && in < static_cast<int>(rows.size()) && row_has(rows[in], a) &&
        row_has(rows[in], b))
    {
        return in;
    }
    for (int r = 0; r < static_cast<int>(rows.size()); r++) {
        if (row_has(rows[r], a) && row_has(rows[r], b)) {
            return r;
        }
    }
    return -1;
}

}  // namespace

PlacedRows<int> tile_rows(const A11ySssState& state) {
    std::vector<int> tiles;
    for (int i = 0; i < A11Y_SSS_TILES; i++) {
        if (drawn(state, i)) {
            tiles.push_back(i);
        }
    }
    std::stable_sort(tiles.begin(), tiles.end(),
        [&](int a, int b) { return state.tiles[a].y > state.tiles[b].y; });
    std::vector<Band> bands;
    for (int i : tiles) {
        const A11ySssTile& tile = state.tiles[i];
        if (bands.empty() || state.tiles[bands.back().tiles.front()].y - tile.y > kLevel) {
            bands.push_back(Band{{}, tile.y - tile.half_height, tile.y + tile.half_height});
        }
        Band& band = bands.back();
        band.tiles.push_back(i);
        band.bottom = std::min(band.bottom, tile.y - tile.half_height);
        band.top = std::max(band.top, tile.y + tile.half_height);
    }
    /* A band reaching into two others is of tall tiles, each in both rows,
     * not a row of its own. */
    std::vector<std::vector<int>> rows(bands.size());
    std::vector<bool> tall(bands.size(), false);
    for (size_t b = 0; b < bands.size(); b++) {
        int reached = 0;
        for (size_t other = 0; other < bands.size(); other++) {
            reached += other != b && overlap(bands[b], bands[other]) >= kReach;
        }
        tall[b] = reached >= 2;
    }
    for (size_t b = 0; b < bands.size(); b++) {
        if (!tall[b]) {
            rows[b].insert(rows[b].end(), bands[b].tiles.begin(), bands[b].tiles.end());
            continue;
        }
        for (size_t other = 0; other < bands.size(); other++) {
            if (other != b && !tall[other] && overlap(bands[b], bands[other]) >= kReach) {
                rows[other].insert(rows[other].end(), bands[b].tiles.begin(), bands[b].tiles.end());
            }
        }
    }
    PlacedRows<int> placed;
    for (size_t b = 0; b < bands.size(); b++) {
        if (tall[b]) {
            continue;
        }
        std::vector<int>& row = rows[b];
        std::stable_sort(row.begin(), row.end(),
            [&](int a, int c) { return state.tiles[a].x < state.tiles[c].x; });
        placed.emplace_back();
        for (int i : row) {
            placed.back().push_back(Placed<int>{i, aim_point(state, i)});
        }
    }
    return placed;
}

int tile_at(const A11ySssState& state, Point at) {
    for (int i = 0; i < A11Y_SSS_TILES; i++) {
        const A11ySssTile& tile = state.tiles[i];
        if (drawn(state, i) && tile.x - tile.half_width < at.x && tile.x + tile.half_width > at.x &&
            tile.y - tile.half_height < at.y && tile.y + tile.half_height > at.y)
        {
            return i;
        }
    }
    return A11Y_SSS_NO_TILE;
}

Point aim_point(const A11ySssState& state, int tile) {
    return Point{state.tiles[tile].x, state.tiles[tile].y};
}

TileStep step(const A11ySssState& state, int from, Point at, Direction direction, int in_row) {
    PlacedRows<int> rows = tile_rows(state);
    if (rows.empty()) {
        return TileStep{};
    }
    if (from == A11Y_SSS_NO_TILE && state.hovered == A11Y_SSS_NO_TILE) {
        int first = rows.front().front().target;
        return TileStep{first, true, 0};
    }
    std::optional<int> to = row_step(rows, from, direction, in_row);
    if (!to) {
        to = nearest_that_way(rows, at, direction);
    }
    if (!to) {
        /* Nothing that way from empty space: A would still pick the tile
         * hovered last. */
        return TileStep{state.hovered, false, -1};
    }
    return TileStep{*to, *to != from, shared_row(rows, from, *to, in_row)};
}

bool may_steer(const A11ySssState& state) {
    return !state.skipped && state.pick == A11Y_SSS_CHOOSING && (state.online || state.owner <= 0);
}

int steering_port(const A11ySssState& state) {
    return std::max(state.owner, 0);
}

Stick stick_as_held(int x, int y) {
    auto axis = [](int value) {
        return value > 0 ? value + kStageDeadZone : value < 0 ? value - kStageDeadZone : 0;
    };
    return Stick{axis(x), axis(y)};
}

}  // namespace a11y
