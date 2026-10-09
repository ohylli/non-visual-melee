/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The shared steps over rows of plain numbered targets. Character select's
 * steps over its own rows are in test_css_targets.cpp; these cover what its
 * screen has none of: a target two rows tall, and an empty row. */
#include "steps.hpp"
#include <cassert>
#include <iostream>
#include <optional>

namespace {

using a11y::Direction;
using a11y::nearest_that_way;
using a11y::Placed;
using a11y::PlacedRows;
using a11y::Point;
using a11y::row_step;

/* Rows like stage select's top: a tall target 1 at the far left in the first
 * two rows, then an empty row, then a row shifted right.
 *
 *     1  2  3  4
 *     1  5  6
 *     (empty)
 *          7  8
 */
PlacedRows<int> rows() {
    return PlacedRows<int>{
        {{1, Point{-10.0f, 9.0f}}, {2, Point{-5.0f, 10.0f}}, {3, Point{0.0f, 10.0f}},
            {4, Point{5.0f, 10.0f}}},
        {{1, Point{-10.0f, 9.0f}}, {5, Point{-5.0f, 6.0f}}, {6, Point{0.0f, 6.0f}}},
        {},
        {{7, Point{-1.0f, 0.0f}}, {8, Point{4.0f, 0.0f}}},
    };
}

void along_a_row_and_its_edges() {
    assert(row_step(rows(), 2, Direction::right) == 3);
    assert(row_step(rows(), 3, Direction::left) == 2);
    assert(row_step(rows(), 4, Direction::right) == 4);
    assert(row_step(rows(), 7, Direction::left) == 7);
    assert(row_step(rows(), 3, Direction::up) == 3);
    assert(row_step(rows(), 8, Direction::down) == 8);
}

void up_and_down_to_the_nearest_in_x() {
    assert(row_step(rows(), 3, Direction::down) == 6);
    assert(row_step(rows(), 4, Direction::down) == 6);
    assert(row_step(rows(), 5, Direction::up) == 2);
    /* Past the empty row. */
    assert(row_step(rows(), 6, Direction::down) == 7);
    assert(row_step(rows(), 8, Direction::up) == 6);
}

void a_target_in_two_rows() {
    /* Left from either row reaches it. */
    assert(row_step(rows(), 2, Direction::left) == 1);
    assert(row_step(rows(), 5, Direction::left) == 1);
    /* Right goes back along the row given, else the topmost. */
    assert(row_step(rows(), 1, Direction::right) == 2);
    assert(row_step(rows(), 1, Direction::right, 0) == 2);
    assert(row_step(rows(), 1, Direction::right, 1) == 5);
    /* A row it is not in is no guide. */
    assert(row_step(rows(), 1, Direction::right, 3) == 2);
    /* Up leaves from its top row, Down from its bottom one. */
    assert(row_step(rows(), 1, Direction::up) == 1);
    assert(row_step(rows(), 1, Direction::down) == 7);
}

void from_no_target_in_the_rows() {
    assert(!row_step(rows(), 9, Direction::left).has_value());
    assert(!row_step(PlacedRows<int>{}, 1, Direction::left).has_value());
}

void from_anywhere_else() {
    /* Between the second row and the bottom one, nearer the bottom one. */
    Point gap{-4.0f, 2.5f};
    assert(nearest_that_way(rows(), gap, Direction::up) == 5);
    assert(nearest_that_way(rows(), gap, Direction::down) == 7);
    /* Along the row nearest in height. */
    assert(nearest_that_way(rows(), gap, Direction::right) == 7);
    /* Nothing left along it: the nearest that way in any row. */
    assert(nearest_that_way(rows(), gap, Direction::left) == 5);
    /* Nothing that way at all. */
    assert(!nearest_that_way(rows(), Point{0.0f, 20.0f}, Direction::up).has_value());
    assert(!nearest_that_way(rows(), Point{-20.0f, 9.0f}, Direction::left).has_value());
    assert(!nearest_that_way(PlacedRows<int>{}, gap, Direction::right).has_value());
}

}  // namespace

int main() {
    along_a_row_and_its_edges();
    up_and_down_to_the_nearest_in_x();
    a_target_in_two_rows();
    from_no_target_in_the_rows();
    from_anywhere_else();
    std::cout << "steps: all tests passed\n";
    return 0;
}
