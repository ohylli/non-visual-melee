/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage select's targets against hand-written snapshots of the screen
 * (test_sss_screen.hpp): its rows, every step from every tile with every
 * stage unlocked and with a fresh save's locks, the tall tiles, the first
 * press, steps from empty space, and when steering is on. */
#include "sss_targets.hpp"
#include "test_sss_screen.hpp"
#include <cassert>
#include <iostream>
#include <iterator>
#include <vector>

namespace {

using a11y::Direction;
using a11y::Point;
using a11y::Stick;
using a11y::TileStep;
using namespace sss_test;

constexpr int kIcicleTile = 22;
constexpr int kFlatZoneTile = 23;
constexpr int kRainbowCruiseTile = 1;
constexpr int kVenomTile = 11;
constexpr int kMushroomKingdomTile = 20;
constexpr int kRandom = A11Y_SSS_RANDOM_TILE;
constexpr int kNone = A11Y_SSS_NO_TILE;

/* Below every tile, where the stage cursor starts. */
constexpr Point kBelow{0.0f, -13.0f};

std::vector<std::vector<int>> tiles_of(const a11y::PlacedRows<int>& rows) {
    std::vector<std::vector<int>> out;
    for (const auto& row : rows) {
        out.emplace_back();
        for (const auto& placed : row) {
            out.back().push_back(placed.target);
        }
    }
    return out;
}

/* The stage cursor has been on a tile since the screen opened. */
A11ySssState browsing(A11ySssState state) {
    state.hovered = kBattlefieldTile;
    return state;
}

/* A step from a tile the stage cursor rests on, at its centre. */
TileStep from_tile(const A11ySssState& state, int tile, Direction direction, int in_row = -1) {
    return a11y::step(state, tile, a11y::aim_point(state, tile), direction, in_row);
}

void rows_as_drawn() {
    using Rows = std::vector<std::vector<int>>;
    assert((tiles_of(a11y::tile_rows(screen())) ==
            Rows{{22, 0, 2, 4, 6, 8, 10, 23}, {22, 1, 3, 5, 7, 9, 11, 23}, {29, 12, 14, 16, 18, 20},
                {13, 15, 17, 19, 21}, {24, 25, 26, 27, 28}}));
    /* Locked tiles are targets; tiles not drawn and their row are gone. */
    assert((tiles_of(a11y::tile_rows(screen_with_locked_stages())) ==
            Rows{{22, 0, 2, 4, 6, 8, 10}, {22, 1, 3, 5, 7, 9, 11}, {29, 12, 14, 16, 18, 20},
                {13, 15, 17, 19, 21}}));
}

/* Where each direction goes from a tile: left, right, up, down. */
struct Expected {
    int from;
    int left;
    int right;
    int up;
    int down;
};

/* Every stage unlocked. */
constexpr Expected kUnlocked[] = {
    /* The top row: Icicle Mountain, Peach's Castle ... Corneria, Flat Zone. */
    {22, 22, 0, 22, 29},
    {0, 22, 2, 0, 1},
    {2, 0, 4, 2, 3},
    {4, 2, 6, 4, 5},
    {6, 4, 8, 6, 7},
    {8, 6, 10, 8, 9},
    {10, 8, 23, 10, 11},
    {23, 10, 23, 23, 20},
    /* The second row: Rainbow Cruise ... Venom. */
    {1, 22, 3, 0, 29},
    {3, 1, 5, 2, 29},
    {5, 3, 7, 4, 12},
    {7, 5, 9, 6, 14},
    {9, 7, 11, 8, 16},
    {11, 9, 23, 10, 18},
    /* The random tile, then Brinstar ... Mushroom Kingdom. */
    {29, 29, 12, 1, 13},
    {12, 29, 14, 5, 13},
    {14, 12, 16, 7, 15},
    {16, 14, 18, 9, 17},
    {18, 16, 20, 11, 19},
    {20, 18, 20, 23, 21},
    /* Brinstar Depths ... Mushroom Kingdom II. */
    {13, 13, 15, 12, 24},
    {15, 13, 17, 14, 24},
    {17, 15, 19, 16, 25},
    {19, 17, 21, 18, 27},
    {21, 19, 21, 20, 28},
    /* Battlefield ... Kongo Jungle N64. */
    {24, 24, 25, 15, 24},
    {25, 24, 26, 17, 25},
    {26, 25, 27, 19, 26},
    {27, 26, 28, 19, 27},
    {28, 27, 28, 21, 28},
};

/* A fresh save: Flat Zone and the bottom row not drawn, the row under the
 * random tile's locked. */
constexpr Expected kFreshSave[] = {
    {22, 22, 0, 22, 29},
    {0, 22, 2, 0, 1},
    {2, 0, 4, 2, 3},
    {4, 2, 6, 4, 5},
    {6, 4, 8, 6, 7},
    {8, 6, 10, 8, 9},
    {10, 8, 10, 10, 11},
    {1, 22, 3, 0, 29},
    {3, 1, 5, 2, 29},
    {5, 3, 7, 4, 12},
    {7, 5, 9, 6, 14},
    {9, 7, 11, 8, 16},
    {11, 9, 11, 10, 18},
    {29, 29, 12, 1, 13},
    {12, 29, 14, 5, 13},
    {14, 12, 16, 7, 15},
    {16, 14, 18, 9, 17},
    {18, 16, 20, 11, 19},
    {20, 18, 20, 11, 21},
    {13, 13, 15, 12, 13},
    {15, 13, 17, 14, 15},
    {17, 15, 19, 16, 17},
    {19, 17, 21, 18, 19},
    {21, 19, 21, 20, 21},
};

template <size_t N>
void check_every_step(const A11ySssState& state, const Expected (&table)[N]) {
    for (const Expected& e : table) {
        int expected[] = {e.left, e.right, e.up, e.down};
        Direction directions[] = {
            Direction::left, Direction::right, Direction::up, Direction::down};
        for (int d = 0; d < 4; d++) {
            TileStep to = from_tile(state, e.from, directions[d]);
            if (to.tile != expected[d]) {
                std::cerr << "from tile " << e.from << ", direction " << d << ": " << to.tile
                          << ", not " << expected[d] << "\n";
            }
            assert(to.tile == expected[d]);
            /* An edge names the tile again and stays. */
            assert(to.moves == (expected[d] != e.from));
        }
    }
}

void every_step_with_every_stage_unlocked() {
    A11ySssState state = browsing(screen());
    check_every_step(state, kUnlocked);
    /* Every drawn tile is covered. */
    assert(std::size(kUnlocked) == A11Y_SSS_TILES);
}

void every_step_with_a_fresh_saves_locks() {
    A11ySssState state = screen_with_locked_stages();
    state.hovered = kOnettTile;
    check_every_step(state, kFreshSave);
    assert(std::size(kFreshSave) == A11Y_SSS_TILES - 6);
}

void the_tall_tiles_from_both_rows() {
    A11ySssState state = browsing(screen());
    /* Left from the top row reaches Icicle Mountain, and Right goes back
     * along it. */
    TileStep to = from_tile(state, kPeachsCastleTile, Direction::left);
    assert(to.tile == kIcicleTile && to.row == 0);
    assert(from_tile(state, kIcicleTile, Direction::right, to.row).tile == kPeachsCastleTile);
    /* From the second row, Right goes back to the second row. */
    to = from_tile(state, kRainbowCruiseTile, Direction::left);
    assert(to.tile == kIcicleTile && to.row == 1);
    assert(from_tile(state, kIcicleTile, Direction::right, to.row).tile == kRainbowCruiseTile);
    /* Flat Zone mirrors it. */
    to = from_tile(state, kCorneriaTile, Direction::right);
    assert(to.tile == kFlatZoneTile && to.row == 0);
    assert(from_tile(state, kFlatZoneTile, Direction::left, to.row).tile == kCorneriaTile);
    to = from_tile(state, kVenomTile, Direction::right);
    assert(to.tile == kFlatZoneTile && to.row == 1);
    assert(from_tile(state, kFlatZoneTile, Direction::left, to.row).tile == kVenomTile);
    /* An edge keeps the row: Right again from Flat Zone, then Left. */
    TileStep edge = from_tile(state, kFlatZoneTile, Direction::right, to.row);
    assert(edge.tile == kFlatZoneTile && !edge.moves && edge.row == 1);
    assert(from_tile(state, kFlatZoneTile, Direction::left, edge.row).tile == kVenomTile);
    /* Never having left the top row: back along it. */
    assert(from_tile(state, kIcicleTile, Direction::right).tile == kPeachsCastleTile);
    /* Down leaves from the second row. */
    assert(from_tile(state, kIcicleTile, Direction::down).tile == kRandom);
    assert(from_tile(state, kFlatZoneTile, Direction::down).tile == kMushroomKingdomTile);
}

void the_first_press_goes_to_icicle_mountain() {
    for (const A11ySssState& state : {screen(), screen_with_locked_stages()}) {
        for (Direction direction :
            {Direction::left, Direction::right, Direction::up, Direction::down})
        {
            TileStep to = a11y::step(state, kNone, kBelow, direction, -1);
            assert(to.tile == kIcicleTile && to.moves && to.row == 0);
        }
    }
    /* A press during that first glide steps on from Icicle Mountain. */
    TileStep to = a11y::step(screen(), kIcicleTile, kBelow, Direction::right, 0);
    assert(to.tile == kPeachsCastleTile);
}

void steps_from_empty_space() {
    A11ySssState state = browsing(screen());
    /* Below every tile: Up to the nearest in x in the bottom row. */
    TileStep to = a11y::step(state, kNone, kBelow, Direction::up, -1);
    assert(to.tile == kBattlefieldTile && to.moves);
    /* Nothing that way: the tile A would pick there, staying put. */
    to = a11y::step(state, kNone, kBelow, Direction::down, -1);
    assert(to.tile == kBattlefieldTile && !to.moves);
    /* The gap between the random tile's row and the one under it, below
     * Onett: Up and Down to the rows either side, Right along the nearer. */
    Point gap{2.0f, 0.9f};
    assert(a11y::tile_at(state, gap) == kNone);
    assert(a11y::step(state, kNone, gap, Direction::up, -1).tile == kOnettTile);
    assert(a11y::step(state, kNone, gap, Direction::down, -1).tile == kFoursideTile);
    assert(a11y::step(state, kNone, gap, Direction::right, -1).tile == 16);
    /* Left of everything: the nearest that way in any row. */
    to = a11y::step(state, kNone, Point{-27.0f, 3.6f}, Direction::left, -1);
    assert(to.tile == kBattlefieldTile && !to.moves);
    assert(a11y::step(state, kNone, Point{-27.0f, 3.6f}, Direction::right, -1).tile == kRandom);
}

void the_cursor_on_a_tile_as_the_game_tests_it() {
    A11ySssState state = screen();
    assert(a11y::tile_at(state, Point{16.499f, 15.7f}) == kCorneriaTile);
    /* Yoshi's Island's box is narrower than its neighbours'. */
    assert(a11y::tile_at(state, Point{5.8f, 10.1f}) == kNone);
    assert(a11y::tile_at(state, Point{5.3f, 10.1f}) == 7);
    /* A tile not drawn is never on. */
    A11ySssState locked = screen_with_locked_stages();
    assert(a11y::tile_at(locked, Point{1.3f, -9.1f}) == kNone);
    assert(a11y::tile_at(locked, Point{2.0f, -1.9f}) == kFoursideTile);
}

void steering_is_for_controller_1() {
    A11ySssState state = screen();
    assert(a11y::may_steer(state));
    state.owner = 0;
    assert(a11y::may_steer(state) && a11y::steering_port(state) == 0);
    /* The Turns and Loser rules hand the cursor to another player. */
    state.owner = 2;
    assert(!a11y::may_steer(state));
    /* Online it is the local player's, on controller 1. */
    state.online = true;
    assert(a11y::may_steer(state) && a11y::steering_port(state) == 2);
    /* Not once a pick is made. */
    state = screen();
    state.pick = A11Y_SSS_PICKED;
    assert(!a11y::may_steer(state));
    state.pick = A11Y_SSS_LEAVING;
    assert(!a11y::may_steer(state));
    state = A11ySssState{};
    state.skipped = true;
    assert(!a11y::may_steer(state));
}

void the_stick_as_the_pad_held_it() {
    assert((a11y::stick_as_held(20, -50) == Stick{50, -80}));
    assert((a11y::stick_as_held(0, 1) == Stick{0, 31}));
    assert((a11y::stick_as_held(0, 0) == Stick{0, 0}));
}

}  // namespace

int main() {
    rows_as_drawn();
    every_step_with_every_stage_unlocked();
    every_step_with_a_fresh_saves_locks();
    the_tall_tiles_from_both_rows();
    the_first_press_goes_to_icicle_mountain();
    steps_from_empty_space();
    the_cursor_on_a_tile_as_the_game_tests_it();
    steering_is_for_controller_1();
    the_stick_as_the_pad_held_it();
    std::cout << "sss_targets: all tests passed\n";
    return 0;
}
