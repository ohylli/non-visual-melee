/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Steps: where a D-pad press goes among a screen's targets
 * (.scratch/character-select/spec.md, "Stage 2: stepping";
 * .scratch/stage-select/spec.md, "Steps" and "Shared code"). A screen lays
 * its targets out in rows, top to bottom, each left to right, every target
 * with an anchor: the point steps measure from and to. Left and Right move
 * along a row, Up and Down to the target nearest in x in the next row, with
 * no wrapping. From a point among no target, the nearest target that way.
 * What a target is stays the screen's own, so this is a template over it;
 * the screen builds the rows and adds its own steps on top (a held slider,
 * a first press). Pure. */
#pragma once
#include "steering.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace a11y {

/* A D-pad direction. */
enum class Direction { left, right, up, down };

/* A target in a row, and where it is in the rows' layout. */
template <typename T>
struct Placed {
    T target;
    Point anchor;
};

/* Rows of targets, top to bottom, each left to right. A target may sit in
 * several rows, as a tile two rows tall does. */
template <typename T>
using PlacedRows = std::vector<std::vector<Placed<T>>>;

namespace steps_detail {

/* From a point among no target, a target counts as on a side of it when it
 * is at least this far that way. */
inline constexpr float kAhead = 0.5f;

/* The target of a row nearest in x to x; the first of two as near. */
template <typename T>
const Placed<T>& nearest_in_x(const std::vector<Placed<T>>& row, float x) {
    const Placed<T>* best = &row.front();
    for (const Placed<T>& placed : row) {
        if (std::fabs(placed.anchor.x - x) < std::fabs(best->anchor.x - x)) {
            best = &placed;
        }
    }
    return *best;
}

/* How far a row is from height y: none within the heights of its targets. */
template <typename T>
float row_distance(const std::vector<Placed<T>>& row, float y) {
    float top = row.front().anchor.y;
    float bottom = top;
    for (const Placed<T>& placed : row) {
        top = std::max(top, placed.anchor.y);
        bottom = std::min(bottom, placed.anchor.y);
    }
    return y > top ? y - top : y < bottom ? bottom - y : 0.0f;
}

template <typename T>
bool row_has(const std::vector<Placed<T>>& row, const T& target) {
    return std::any_of(
        row.begin(), row.end(), [&](const Placed<T>& placed) { return placed.target == target; });
}

}  // namespace steps_detail

/* A step from a target in the rows: along its row to the next target, Up and
 * Down to the target nearest in x in the next row that has any; from itself
 * at an edge. A target in several rows leaves Up from the topmost of them
 * and Down from the bottommost, and moves Left and Right along in_row when
 * it is one of them (the row the player came from), else along the topmost.
 * Nothing when from is in no row. */
template <typename T>
std::optional<T> row_step(
    const PlacedRows<T>& rows, const T& from, Direction direction, int in_row = -1) {
    int top = -1;
    int bottom = -1;
    for (int r = 0; r < static_cast<int>(rows.size()); r++) {
        if (steps_detail::row_has(rows[r], from)) {
            top = top < 0 ? r : top;
            bottom = r;
        }
    }
    if (top < 0) {
        return std::nullopt;
    }
    bool along = in_row >= 0 && in_row < static_cast<int>(rows.size()) &&
                 steps_detail::row_has(rows[in_row], from);
    const std::vector<Placed<T>>& row = rows[along ? in_row : top];
    size_t column = 0;
    while (!(row[column].target == from)) {
        column++;
    }
    float x = row[column].anchor.x;
    switch (direction) {
    case Direction::left:
        return column > 0 ? row[column - 1].target : from;
    case Direction::right:
        return column + 1 < row.size() ? row[column + 1].target : from;
    case Direction::up:
        for (int r = top - 1; r >= 0; r--) {
            if (!rows[r].empty()) {
                return steps_detail::nearest_in_x(rows[r], x).target;
            }
        }
        return from;
    case Direction::down:
        for (int r = bottom + 1; r < static_cast<int>(rows.size()); r++) {
            if (!rows[r].empty()) {
                return steps_detail::nearest_in_x(rows[r], x).target;
            }
        }
        return from;
    }
    return std::nullopt;
}

/* A step from a point among no target: the nearest target on the
 * direction's side of it. Up and Down: the row with the target nearest that
 * way in height, and in it the target nearest in x among those that way.
 * Left and Right: along the row the point is in or nearest to, else the
 * nearest target that way in any row. Nothing when nothing lies that way. */
template <typename T>
std::optional<T> nearest_that_way(const PlacedRows<T>& rows, Point at, Direction direction) {
    using steps_detail::kAhead;
    if (direction == Direction::up || direction == Direction::down) {
        float way = direction == Direction::up ? 1.0f : -1.0f;
        std::vector<Placed<T>> best;
        float best_ahead = 0.0f;
        for (const std::vector<Placed<T>>& row : rows) {
            std::vector<Placed<T>> that_way;
            float ahead = 0.0f;
            for (const Placed<T>& placed : row) {
                float by = (placed.anchor.y - at.y) * way;
                if (by > kAhead) {
                    ahead = that_way.empty() ? by : std::min(ahead, by);
                    that_way.push_back(placed);
                }
            }
            if (!that_way.empty() && (best.empty() || ahead < best_ahead)) {
                best = that_way;
                best_ahead = ahead;
            }
        }
        if (best.empty()) {
            return std::nullopt;
        }
        return steps_detail::nearest_in_x(best, at.x).target;
    }
    float way = direction == Direction::right ? 1.0f : -1.0f;
    const Placed<T>* best = nullptr;
    float best_distance = 0.0f;
    /* The target, if it lies that way and nearer than the best so far. */
    auto consider = [&](const Placed<T>& placed, float distance) {
        if ((placed.anchor.x - at.x) * way > kAhead &&
            (best == nullptr || distance < best_distance))
        {
            best = &placed;
            best_distance = distance;
        }
    };
    const std::vector<Placed<T>>* nearest_row = nullptr;
    for (const std::vector<Placed<T>>& row : rows) {
        if (!row.empty() &&
            (nearest_row == nullptr || steps_detail::row_distance(row, at.y) <
                                           steps_detail::row_distance(*nearest_row, at.y)))
        {
            nearest_row = &row;
        }
    }
    if (nearest_row == nullptr) {
        return std::nullopt;
    }
    for (const Placed<T>& placed : *nearest_row) {
        consider(placed, std::fabs(placed.anchor.x - at.x));
    }
    if (best == nullptr) {
        for (const std::vector<Placed<T>>& row : rows) {
            for (const Placed<T>& placed : row) {
                consider(placed, std::hypot(placed.anchor.x - at.x, placed.anchor.y - at.y));
            }
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return best->target;
}

}  // namespace a11y
