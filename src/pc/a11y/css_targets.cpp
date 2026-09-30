/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_targets.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace a11y {
namespace {

/* The top bar's buttons, as mnCharSel_CursorThink tests them: above y 22,
 * Back right of x 17.3, the rules header between x -17 and 15, Teams left of
 * x -25.5. */
constexpr float kTopBarBottom = 22.0f;
constexpr float kBackLeft = 17.3f;
constexpr float kRulesLeft = -17.0f;
constexpr float kRulesRight = 15.0f;
constexpr float kTeamsRight = -25.5f;
/* A free hand grabs a slider's knob within this squared distance. */
constexpr float kGrabDistanceSquared = 5.0f;
/* Portraits whose tops are this close are in one row. */
constexpr float kSameRow = 0.5f;
/* From anywhere else, a target counts as on a side of the hand when it is at
 * least this far that way. */
constexpr float kAhead = 0.5f;

bool inside(const A11yCssRect& rect, float x, float y) {
    return x > rect.left && x < rect.right && y < rect.top && y > rect.bottom;
}

bool near(const A11yCssKnob& knob, float x, float y) {
    float dx = x - knob.x;
    float dy = y - knob.y;
    return knob.known && dx * dx + dy * dy < kGrabDistanceSquared;
}

Target portrait_at(const A11yCssState& state, float x, float y) {
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        const A11yCssPortrait& portrait = state.portraits[i];
        if (!portrait.locked && inside(portrait.rect, x + kCoinOffsetX, y + kCoinOffsetY)) {
            return Target{TargetKind::portrait, i};
        }
    }
    return Target{};
}

Target top_bar_at(const A11yCssState& state, float x, float y) {
    if (y <= kTopBarBottom) {
        return Target{};
    }
    if (x > kBackLeft) {
        return Target{TargetKind::back};
    }
    if (state.has_rules_button && x > kRulesLeft && x < kRulesRight) {
        return Target{TargetKind::rules};
    }
    if (state.has_teams_button && x < kTeamsRight) {
        return Target{TargetKind::teams};
    }
    return Target{};
}

/* The player slots' buttons in VS modes, slot by slot, each slot's in the
 * order the game tries them; the name box after every slot. */
Target slot_target_at(const A11yCssState& state, float x, float y) {
    for (int i = 0; i < state.slot_count && i < A11Y_CSS_SLOTS; i++) {
        const A11yCssSlot& slot = state.slots[i];
        /* The HMN/CPU button waits while anything of the slot is in a hand,
         * and while its name tag window is open. */
        if (!slot.cpu_level_held && !slot.handicap_held && !slot.carried && !slot.hand_holding &&
            !slot.name_tags_open && inside(slot.slot_button, x, y))
        {
            return Target{TargetKind::slot_button, i};
        }
        if (state.teams && slot.kind != A11Y_CSS_CLOSED && inside(slot.team_button, x, y)) {
            return Target{TargetKind::team_button, i};
        }
        if (!slot.cpu_level_held && slot.kind == A11Y_CSS_CPU && near(slot.cpu_level_knob, x, y)) {
            return Target{TargetKind::cpu_level, i};
        }
        /* A player sets their own handicap; anyone sets a CPU's. */
        if (!slot.handicap_held && state.handicap_sliders && slot.kind != A11Y_CSS_CLOSED &&
            (slot.kind == A11Y_CSS_CPU || i == state.local_slot) && near(slot.handicap_knob, x, y))
        {
            return Target{TargetKind::handicap, i};
        }
    }
    int own = state.local_slot;
    if (own < state.slot_count && state.slots[own].kind == A11Y_CSS_HUMAN &&
        inside(state.slots[own].name_box, x, y))
    {
        return Target{TargetKind::name_box, own};
    }
    return Target{};
}

float centre_x(const A11yCssRect& rect) {
    return (rect.left + rect.right) / 2.0f;
}

float centre_y(const A11yCssRect& rect) {
    return (rect.top + rect.bottom) / 2.0f;
}

/* The unlocked portraits in rows as drawn, top to bottom, each left to
 * right. Built from the rectangles, so a portrait the game moved is where it
 * is drawn: with Luigi locked, Luigi's and Pikachu's trade rows. */
std::vector<std::vector<int>> portrait_rows(const A11yCssState& state) {
    std::vector<int> portraits;
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        if (!state.portraits[i].locked) {
            portraits.push_back(i);
        }
    }
    std::sort(portraits.begin(), portraits.end(), [&](int a, int b) {
        const A11yCssRect& first = state.portraits[a].rect;
        const A11yCssRect& second = state.portraits[b].rect;
        if (std::fabs(first.top - second.top) > kSameRow) {
            return first.top > second.top;
        }
        return first.left < second.left;
    });
    std::vector<std::vector<int>> rows;
    float row_top = 0.0f;
    for (int i : portraits) {
        float top = state.portraits[i].rect.top;
        if (rows.empty() || std::fabs(top - row_top) > kSameRow) {
            rows.emplace_back();
            row_top = top;
        }
        rows.back().push_back(i);
    }
    return rows;
}

/* The portrait of row nearest in x to x; the leftmost of two as near. */
int nearest_in_x(const A11yCssState& state, const std::vector<int>& row, float x) {
    int best = row.front();
    for (int i : row) {
        if (std::fabs(centre_x(state.portraits[i].rect) - x) <
            std::fabs(centre_x(state.portraits[best].rect) - x))
        {
            best = i;
        }
    }
    return best;
}

/* A step from the portrait from along the rows; from itself at an edge, none
 * when from is not among them. */
Target step_from_portrait(const A11yCssState& state, Target from, Direction direction) {
    std::vector<std::vector<int>> rows = portrait_rows(state);
    for (size_t r = 0; r < rows.size(); r++) {
        const std::vector<int>& row = rows[r];
        auto found = std::find(row.begin(), row.end(), from.index);
        if (found == row.end()) {
            continue;
        }
        size_t column = static_cast<size_t>(found - row.begin());
        float x = centre_x(state.portraits[from.index].rect);
        switch (direction) {
        case Direction::left:
            return column > 0 ? Target{TargetKind::portrait, row[column - 1]} : from;
        case Direction::right:
            return column + 1 < row.size() ? Target{TargetKind::portrait, row[column + 1]} : from;
        case Direction::up:
            return r > 0 ? Target{TargetKind::portrait, nearest_in_x(state, rows[r - 1], x)} : from;
        case Direction::down:
            return r + 1 < rows.size() ?
                       Target{TargetKind::portrait, nearest_in_x(state, rows[r + 1], x)} :
                       from;
        }
    }
    return Target{};
}

/* The row a hand at height y is in or nearest to: the coin within its
 * portraits' height, or the least far from it. */
const std::vector<int>& nearest_row(
    const A11yCssState& state, const std::vector<std::vector<int>>& rows, float y) {
    float coin = y + kCoinOffsetY;
    auto distance = [&](const std::vector<int>& row) {
        const A11yCssRect& rect = state.portraits[row.front()].rect;
        return coin > rect.top ? coin - rect.top : coin < rect.bottom ? rect.bottom - coin : 0.0f;
    };
    const std::vector<int>* best = &rows.front();
    for (const std::vector<int>& row : rows) {
        if (distance(row) < distance(*best)) {
            best = &row;
        }
    }
    return *best;
}

/* The nearest target on the direction's side of the hand, or none, by rows
 * as a step from a portrait goes: Up and Down to the nearest row that way,
 * at the portrait nearest in x; Left and Right along the row the hand is in
 * or nearest to, else to the nearest portrait that way in any row. */
Target nearest_that_way(const A11yCssState& state, float x, float y, Direction direction) {
    std::vector<std::vector<int>> rows = portrait_rows(state);
    if (rows.empty()) {
        return Target{};
    }
    if (direction == Direction::up || direction == Direction::down) {
        float way = direction == Direction::up ? 1.0f : -1.0f;
        const std::vector<int>* best = nullptr;
        float best_ahead = 0.0f;
        for (const std::vector<int>& row : rows) {
            float ahead = (aim_point(state, Target{TargetKind::portrait, row.front()}).y - y) * way;
            if (ahead > kAhead && (best == nullptr || ahead < best_ahead)) {
                best = &row;
                best_ahead = ahead;
            }
        }
        return best != nullptr ?
                   Target{TargetKind::portrait, nearest_in_x(state, *best, x + kCoinOffsetX)} :
                   Target{};
    }
    float way = direction == Direction::right ? 1.0f : -1.0f;
    Target best;
    float best_distance = 0.0f;
    auto consider = [&](int i, float distance) {
        if ((aim_point(state, Target{TargetKind::portrait, i}).x - x) * way > kAhead &&
            (best.kind == TargetKind::none || distance < best_distance))
        {
            best = Target{TargetKind::portrait, i};
            best_distance = distance;
        }
    };
    for (int i : nearest_row(state, rows, y)) {
        consider(i, std::fabs(aim_point(state, Target{TargetKind::portrait, i}).x - x));
    }
    if (best.kind != TargetKind::none) {
        return best;
    }
    for (const std::vector<int>& row : rows) {
        for (int i : row) {
            Point aim = aim_point(state, Target{TargetKind::portrait, i});
            consider(i, std::hypot(aim.x - x, aim.y - y));
        }
    }
    return best;
}

}  // namespace

Point aim_point(const A11yCssState& state, Target target) {
    if (target.kind != TargetKind::portrait) {
        return Point{};
    }
    /* Portraits are tested at the coin, so the hand aims to put the coin at
     * the centre, carried or not. */
    const A11yCssRect& rect = state.portraits[target.index].rect;
    return Point{centre_x(rect) - kCoinOffsetX, centre_y(rect) - kCoinOffsetY};
}

Target step(const A11yCssState& state, Target from, float x, float y, Direction direction) {
    if (from.kind == TargetKind::portrait) {
        Target to = step_from_portrait(state, from, direction);
        if (to.kind != TargetKind::none) {
            return to;
        }
    }
    return nearest_that_way(state, x, y, direction);
}

Target target_at(const A11yCssState& state, float x, float y) {
    if (state.hand.slider != A11Y_CSS_NO_SLIDER || state.slots[state.local_slot].name_tags_open) {
        return Target{};
    }
    if (state.hand.coin < 0) {
        Target target = top_bar_at(state, x, y);
        /* Single-player modes' own buttons come with their reader. */
        if (target.kind == TargetKind::none && state.hand_count == A11Y_CSS_SLOTS) {
            target = slot_target_at(state, x, y);
        }
        if (target.kind != TargetKind::none) {
            return target;
        }
    }
    return portrait_at(state, x, y);
}

}  // namespace a11y
