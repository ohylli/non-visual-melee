/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_targets.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

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
/* The bounds the game keeps the hand inside. */
constexpr float kHandLeft = -35.0f;
constexpr float kHandRight = 26.0f;
constexpr float kHandTop = 25.0f;
/* A free hand grabs a slider's knob within this squared distance. */
constexpr float kGrabDistanceSquared = 5.0f;
/* A on a free hand picks up the nearest coin within 3 units of the hand plus
 * (3.8, -2.6), if the hand is in the portrait band, below y 22. A coin
 * resting at the very top of the top row would put that point above the
 * band, so the aim stays below this, still within reach. */
constexpr float kPickupOffsetX = 3.8f;
constexpr float kPickupOffsetY = -2.6f;
constexpr float kPickupAimTop = 21.0f;
/* A held slider's value (updateGrabbedSlider): the knob's place along its 10
 * units, times 0.8, plus 0.5, truncated, plus 1. So value v is centred
 * (v - 1) * 1.25 units along and spans 1.25 units, the two ends' cut off by
 * the slider's ends. */
constexpr float kSliderLength = 10.0f;
constexpr float kLevelWidth = 1.25f;
/* Portraits whose tops are this close are in one row. */
constexpr float kSameRow = 0.5f;
/* From anywhere else, a target counts as on a side of the hand when it is at
 * least this far that way. */
constexpr float kAhead = 0.5f;

using Rows = std::vector<std::vector<Target>>;

bool inside(const A11yCssRect& rect, float x, float y) {
    return x > rect.left && x < rect.right && y < rect.top && y > rect.bottom;
}

bool near(const A11yCssKnob& knob, float x, float y) {
    float dx = x - knob.x;
    float dy = y - knob.y;
    return knob.known && dx * dx + dy * dy < kGrabDistanceSquared;
}

constexpr SliderKind kSliders[] = {
    {A11Y_CSS_CPU_LEVEL, TargetKind::cpu_level, &A11yCssSlot::cpu_level_knob,
        &A11yCssSlot::cpu_level},
    {A11Y_CSS_HANDICAP, TargetKind::handicap, &A11yCssSlot::handicap_knob, &A11yCssSlot::handicap},
};

const A11yCssKnob& knob_of(const A11yCssState& state, Target target) {
    return state.slots[target.index].*slider_kind(target.kind)->knob;
}

/* The slider the hand holds, on its value. */
Target held_slider(const A11yCssState& state) {
    const SliderKind* held = slider_kind(state.hand.slider);
    int slot = state.hand.slider_slot;
    return Target{held->knob_kind, slot, state.slots[slot].*held->value};
}

/* The middle of the span of a slider's value, from its lowest end. */
float level_middle(int level) {
    float centre = static_cast<float>(level - kLowestLevel) * kLevelWidth;
    float low = std::max(0.0f, centre - kLevelWidth / 2.0f);
    float high = std::min(kSliderLength, centre + kLevelWidth / 2.0f);
    return (low + high) / 2.0f;
}

float centre_x(const A11yCssRect& rect) {
    return (rect.left + rect.right) / 2.0f;
}

float centre_y(const A11yCssRect& rect) {
    return (rect.top + rect.bottom) / 2.0f;
}

Point centre(const A11yCssRect& rect) {
    return Point{centre_x(rect), centre_y(rect)};
}

/* Where the game tests the hand for an arrow. */
const A11yCssRect& arrow_rect(const A11yCssState& state, Target arrow) {
    const A11yCssArrows& arrows = state.arrows;
    switch (arrow.kind) {
    case TargetKind::difficulty_lower:
        return arrows.difficulty_lower;
    case TargetKind::difficulty_higher:
        return arrows.difficulty_higher;
    case TargetKind::stocks_fewer:
        return arrows.stocks_fewer;
    default:
        break;
    }
    return arrows.stocks_more;
}

/* The point steps measure from and to: where a target is in the rows' layout.
 * It is the aim, apart from a coin to pick up or a slider's value, which move
 * the aim without moving the target in the rows. For a portrait, where the
 * hand puts the coin at its centre; for a top bar button, the middle of its
 * area within the hand's bounds; for a button, its middle; for a knob, where
 * it is grabbed. */
Point step_anchor(const A11yCssState& state, Target target) {
    constexpr float kTopBarMiddle = (kTopBarBottom + kHandTop) / 2.0f;
    switch (target.kind) {
    case TargetKind::none:
        break;
    case TargetKind::portrait: {
        Point middle = centre(state.portraits[target.index].rect);
        return Point{middle.x - kCoinOffsetX, middle.y - kCoinOffsetY};
    }
    case TargetKind::slot_button:
        return centre(state.slots[target.index].slot_button);
    case TargetKind::team_button:
        return centre(state.slots[target.index].team_button);
    case TargetKind::cpu_level:
    case TargetKind::handicap: {
        const A11yCssKnob& knob = knob_of(state, target);
        return Point{knob.x, knob.y};
    }
    case TargetKind::name_box:
        return centre(state.slots[target.index].name_box);
    case TargetKind::teams:
        return Point{(kHandLeft + kTeamsRight) / 2.0f, kTopBarMiddle};
    case TargetKind::rules:
        return Point{(kRulesLeft + kRulesRight) / 2.0f, kTopBarMiddle};
    case TargetKind::back:
        return Point{(kBackLeft + kHandRight) / 2.0f, kTopBarMiddle};
    case TargetKind::difficulty_lower:
    case TargetKind::difficulty_higher:
    case TargetKind::stocks_fewer:
    case TargetKind::stocks_more:
        return centre(arrow_rect(state, target));
    }
    return Point{};
}

/* The hand at (x, y) is on the target, by the area the game tests for it,
 * whatever the game would do there now. */
bool contains(const A11yCssState& state, Target target, float x, float y) {
    switch (target.kind) {
    case TargetKind::none:
        break;
    case TargetKind::portrait:
        return inside(state.portraits[target.index].rect, x + kCoinOffsetX, y + kCoinOffsetY);
    case TargetKind::slot_button:
        return inside(state.slots[target.index].slot_button, x, y);
    case TargetKind::team_button:
        return inside(state.slots[target.index].team_button, x, y);
    case TargetKind::cpu_level:
    case TargetKind::handicap:
        return near(knob_of(state, target), x, y);
    case TargetKind::name_box:
        return inside(state.slots[target.index].name_box, x, y);
    case TargetKind::teams:
        return y > kTopBarBottom && x < kTeamsRight;
    case TargetKind::rules:
        return y > kTopBarBottom && x > kRulesLeft && x < kRulesRight;
    case TargetKind::back:
        return y > kTopBarBottom && x > kBackLeft;
    case TargetKind::difficulty_lower:
    case TargetKind::difficulty_higher:
    case TargetKind::stocks_fewer:
    case TargetKind::stocks_more:
        return inside(arrow_rect(state, target), x, y);
    }
    return false;
}

/* A free hand's A on the target would act now: a slot's HMN/CPU button waits
 * while anything of the slot is in a hand and while its name tag window is
 * open, and a slider held by another hand cannot be grabbed. */
bool reacts(const A11yCssState& state, Target target) {
    switch (target.kind) {
    case TargetKind::slot_button: {
        const A11yCssSlot& slot = state.slots[target.index];
        return !slot.cpu_level_held && !slot.handicap_held && !slot.carried && !slot.hand_holding &&
               !slot.name_tags_open;
    }
    case TargetKind::cpu_level:
        return !state.slots[target.index].cpu_level_held;
    case TargetKind::handicap:
        return !state.slots[target.index].handicap_held;
    case TargetKind::none:
    case TargetKind::portrait:
    case TargetKind::team_button:
    case TargetKind::name_box:
    case TargetKind::teams:
    case TargetKind::rules:
    case TargetKind::back:
    case TargetKind::difficulty_lower:
    case TargetKind::difficulty_higher:
    case TargetKind::stocks_fewer:
    case TargetKind::stocks_more:
        break;
    }
    return true;
}

/* The first of the targets that contains (x, y) and would react; none if no
 * target does. */
Target first_at(const A11yCssState& state, const std::vector<Target>& targets, float x, float y) {
    for (Target target : targets) {
        if (contains(state, target, x, y) && reacts(state, target)) {
            return target;
        }
    }
    return Target{};
}

/* The top bar's buttons that exist, left to right. */
std::vector<Target> top_bar_row(const A11yCssState& state) {
    std::vector<Target> row;
    if (state.has_teams_button) {
        row.push_back(Target{TargetKind::teams});
    }
    if (state.has_rules_button) {
        row.push_back(Target{TargetKind::rules});
    }
    row.push_back(Target{TargetKind::back});
    return row;
}

/* The unlocked portraits in the game's table order, the order it tests them
 * in. */
std::vector<Target> unlocked_portraits(const A11yCssState& state) {
    std::vector<Target> portraits;
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        if (!state.portraits[i].locked) {
            portraits.push_back(Target{TargetKind::portrait, i});
        }
    }
    return portraits;
}

/* The unlocked portraits in rows as drawn, top to bottom, each left to
 * right. Built from the rectangles, so a portrait the game moved is where it
 * is drawn: with Luigi locked, Luigi's and Pikachu's trade rows. */
Rows portrait_rows(const A11yCssState& state) {
    std::vector<Target> portraits = unlocked_portraits(state);
    std::sort(portraits.begin(), portraits.end(), [&](Target a, Target b) {
        const A11yCssRect& first = state.portraits[a.index].rect;
        const A11yCssRect& second = state.portraits[b.index].rect;
        if (std::fabs(first.top - second.top) > kSameRow) {
            return first.top > second.top;
        }
        return first.left < second.left;
    });
    Rows rows;
    float row_top = 0.0f;
    for (Target portrait : portraits) {
        float top = state.portraits[portrait.index].rect.top;
        if (rows.empty() || std::fabs(top - row_top) > kSameRow) {
            rows.emplace_back();
            row_top = top;
        }
        rows.back().push_back(portrait);
    }
    return rows;
}

/* The player slots' targets that exist now, slot by slot, each slot's in the
 * order the game tries them, by the conditions of the game's tests; for a
 * knob, also that the model has it. */
std::vector<Target> slot_row(const A11yCssState& state) {
    std::vector<Target> row;
    for (int i = 0; i < state.slot_count && i < A11Y_CSS_SLOTS; i++) {
        const A11yCssSlot& slot = state.slots[i];
        bool open = slot.kind != A11Y_CSS_CLOSED;
        bool cpu = slot.kind == A11Y_CSS_CPU;
        row.push_back(Target{TargetKind::slot_button, i});
        if (state.teams && open) {
            row.push_back(Target{TargetKind::team_button, i});
        }
        if (cpu && slot.cpu_level_knob.known) {
            row.push_back(Target{TargetKind::cpu_level, i});
        }
        /* A player sets their own handicap; anyone sets a CPU's. */
        if (state.handicap_sliders && open && (cpu || i == state.local_slot) &&
            slot.handicap_knob.known)
        {
            row.push_back(Target{TargetKind::handicap, i});
        }
    }
    return row;
}

/* A single-player mode's arrows as drawn: the difficulty row above the stock
 * row, each arrow pointing down the range on the left. */
Rows arrow_rows(const A11yCssState& state) {
    Rows rows;
    if (state.arrows.difficulty_shown) {
        rows.push_back(
            {Target{TargetKind::difficulty_lower}, Target{TargetKind::difficulty_higher}});
    }
    if (state.arrows.stocks_shown) {
        rows.push_back({Target{TargetKind::stocks_fewer}, Target{TargetKind::stocks_more}});
    }
    return rows;
}

/* The arrows in the order mnCharSel_CursorThink tests them: the stock arrows
 * first. */
std::vector<Target> arrows_in_test_order(const A11yCssState& state) {
    std::vector<Target> arrows;
    for (const std::vector<Target>& row : arrow_rows(state)) {
        arrows.insert(arrows.begin(), row.begin(), row.end());
    }
    return arrows;
}

/* The target of row nearest in x to x; the first of two as near. */
Target nearest_in_x(const A11yCssState& state, const std::vector<Target>& row, float x) {
    Target best = row.front();
    for (Target target : row) {
        if (std::fabs(step_anchor(state, target).x - x) < std::fabs(step_anchor(state, best).x - x))
        {
            best = target;
        }
    }
    return best;
}

/* A step from a target in the rows; nothing when from is not among them. */
std::optional<Target> row_step(
    const A11yCssState& state, const Rows& rows, Target from, Direction direction) {
    if (from.kind == TargetKind::none) {
        return std::nullopt;
    }
    for (size_t r = 0; r < rows.size(); r++) {
        const std::vector<Target>& row = rows[r];
        auto found = std::find(row.begin(), row.end(), from);
        if (found == row.end()) {
            continue;
        }
        size_t column = static_cast<size_t>(found - row.begin());
        float x = step_anchor(state, from).x;
        switch (direction) {
        case Direction::left:
            return column > 0 ? row[column - 1] : from;
        case Direction::right:
            return column + 1 < row.size() ? row[column + 1] : from;
        case Direction::up:
            return r > 0 ? nearest_in_x(state, rows[r - 1], x) : from;
        case Direction::down:
            return r + 1 < rows.size() ? nearest_in_x(state, rows[r + 1], x) : from;
        }
    }
    return std::nullopt;
}

/* How far a row is from height y: none within the heights of its targets. */
float row_distance(const A11yCssState& state, const std::vector<Target>& row, float y) {
    float top = step_anchor(state, row.front()).y;
    float bottom = top;
    for (Target target : row) {
        top = std::max(top, step_anchor(state, target).y);
        bottom = std::min(bottom, step_anchor(state, target).y);
    }
    return y > top ? y - top : y < bottom ? bottom - y : 0.0f;
}

/* The nearest target on the direction's side of the hand, or none. Up and
 * Down: the row with the target nearest that way in height, and in it the
 * target nearest in x among those that way. Left and Right: along the row the
 * hand is in or nearest to, else the nearest target that way in any row. */
Target nearest_that_way(
    const A11yCssState& state, const Rows& rows, float x, float y, Direction direction) {
    if (direction == Direction::up || direction == Direction::down) {
        float way = direction == Direction::up ? 1.0f : -1.0f;
        std::vector<Target> best;
        float best_ahead = 0.0f;
        for (const std::vector<Target>& row : rows) {
            std::vector<Target> that_way;
            float ahead = 0.0f;
            for (Target target : row) {
                float by = (step_anchor(state, target).y - y) * way;
                if (by > kAhead) {
                    ahead = that_way.empty() ? by : std::min(ahead, by);
                    that_way.push_back(target);
                }
            }
            if (!that_way.empty() && (best.empty() || ahead < best_ahead)) {
                best = that_way;
                best_ahead = ahead;
            }
        }
        return best.empty() ? Target{} : nearest_in_x(state, best, x);
    }
    if (rows.empty()) {
        return Target{};
    }
    float way = direction == Direction::right ? 1.0f : -1.0f;
    Target best;
    float best_distance = 0.0f;
    /* The target, at its anchor, if it lies that way and nearer than the best so
     * far. */
    auto consider = [&](Target target, Point at, float distance) {
        if ((at.x - x) * way > kAhead &&
            (best.kind == TargetKind::none || distance < best_distance))
        {
            best = target;
            best_distance = distance;
        }
    };
    const std::vector<Target>* nearest_row = &rows.front();
    for (const std::vector<Target>& row : rows) {
        if (row_distance(state, row, y) < row_distance(state, *nearest_row, y)) {
            nearest_row = &row;
        }
    }
    for (Target target : *nearest_row) {
        Point at = step_anchor(state, target);
        consider(target, at, std::fabs(at.x - x));
    }
    if (best.kind != TargetKind::none) {
        return best;
    }
    for (const std::vector<Target>& row : rows) {
        for (Target target : row) {
            Point at = step_anchor(state, target);
            consider(target, at, std::hypot(at.x - x, at.y - y));
        }
    }
    return best;
}

/* A step of the held slider: Left and Right one value, not past the ends; Up
 * and Down keep it. From the value a glide goes to, else the slider's own. */
Target slider_step(const A11yCssState& state, Target from, Direction direction) {
    Target to = held_slider(state);
    if (from.kind == to.kind && from.index == to.index && from.level) {
        to.level = from.level;
    }
    if (direction == Direction::left) {
        to.level = std::max(kLowestLevel, *to.level - 1);
    } else if (direction == Direction::right) {
        to.level = std::min(kHighestLevel, *to.level + 1);
    }
    return to;
}

}  // namespace

Area area(Target target) {
    switch (target.kind) {
    case TargetKind::none:
        break;
    case TargetKind::portrait:
        return Area::portraits;
    case TargetKind::slot_button:
    case TargetKind::team_button:
    case TargetKind::cpu_level:
    case TargetKind::handicap:
    case TargetKind::name_box:
        return Area::player_slots;
    case TargetKind::teams:
    case TargetKind::rules:
    case TargetKind::back:
        return Area::top_bar;
    /* Below the portraits, where a carried coin goes back. */
    case TargetKind::difficulty_lower:
    case TargetKind::difficulty_higher:
    case TargetKind::stocks_fewer:
    case TargetKind::stocks_more:
        return Area::player_slots;
    }
    return Area::none;
}

const SliderKind* slider_kind(A11yCssSlider slider) {
    for (const SliderKind& kind : kSliders) {
        if (kind.slider == slider) {
            return &kind;
        }
    }
    return nullptr;
}

const SliderKind* slider_kind(TargetKind knob) {
    for (const SliderKind& kind : kSliders) {
        if (kind.knob_kind == knob) {
            return &kind;
        }
    }
    return nullptr;
}

std::vector<std::vector<Target>> target_rows(const A11yCssState& state) {
    Rows rows;
    rows.push_back(top_bar_row(state));
    for (std::vector<Target>& row : portrait_rows(state)) {
        rows.push_back(std::move(row));
    }
    if (state.hand_count == A11Y_CSS_SLOTS) {
        std::vector<Target> slots = slot_row(state);
        if (!slots.empty()) {
            rows.push_back(std::move(slots));
        }
    } else {
        for (std::vector<Target>& row : arrow_rows(state)) {
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

Target locate(const A11yCssState& state, float x, float y) {
    if (state.hand.slider != A11Y_CSS_NO_SLIDER) {
        return held_slider(state);
    }
    for (const std::vector<Target>& row : target_rows(state)) {
        for (Target target : row) {
            if (contains(state, target, x, y)) {
                return target;
            }
        }
    }
    return Target{};
}

int pickable_coin(const A11yCssState& state, int portrait) {
    if (state.hand.coin >= 0 || state.hand.slider != A11Y_CSS_NO_SLIDER) {
        return -1;
    }
    int own = -1;
    for (int i = 0; i < A11Y_CSS_SLOTS; i++) {
        const A11yCssSlot& slot = state.slots[i];
        bool mine = i == state.local_slot;
        if (slot.kind == A11Y_CSS_CLOSED || slot.carried || !slot.coin.seen ||
            slot.portrait != portrait || (slot.kind != A11Y_CSS_CPU && !mine))
        {
            continue;
        }
        if (!mine) {
            return i;
        }
        own = i;
    }
    return own;
}

Point aim_point(const A11yCssState& state, Target target) {
    if (target.level) {
        const A11yCssKnob& knob = knob_of(state, target);
        return Point{knob.origin_x + level_middle(*target.level), knob.y};
    }
    if (target.kind == TargetKind::portrait) {
        int coin = pickable_coin(state, target.index);
        if (coin >= 0) {
            const A11yCssSlot& slot = state.slots[coin];
            return Point{slot.coin.x - kPickupOffsetX,
                std::min(slot.coin.y - kPickupOffsetY, kPickupAimTop)};
        }
    }
    return step_anchor(state, target);
}

bool is_arrow(Target target) {
    switch (target.kind) {
    case TargetKind::difficulty_lower:
    case TargetKind::difficulty_higher:
    case TargetKind::stocks_fewer:
    case TargetKind::stocks_more:
        return true;
    default:
        break;
    }
    return false;
}

bool arrow_at_end(const A11yCssState& state, Target arrow) {
    const A11yCssArrows& arrows = state.arrows;
    switch (arrow.kind) {
    case TargetKind::difficulty_lower:
        return arrows.difficulty <= A11Y_CSS_LOWEST_DIFFICULTY;
    case TargetKind::difficulty_higher:
        return arrows.difficulty >= A11Y_CSS_HIGHEST_DIFFICULTY;
    case TargetKind::stocks_fewer:
        return arrows.stocks <= A11Y_CSS_FEWEST_STOCKS;
    case TargetKind::stocks_more:
        return arrows.stocks >= A11Y_CSS_MOST_STOCKS;
    default:
        break;
    }
    return false;
}

Target step(const A11yCssState& state, Target from, float x, float y, Direction direction) {
    if (state.hand.slider != A11Y_CSS_NO_SLIDER) {
        return slider_step(state, from, direction);
    }
    Rows rows = target_rows(state);
    if (std::optional<Target> to = row_step(state, rows, from, direction)) {
        return *to;
    }
    if (std::optional<Target> to = row_step(state, rows, locate(state, x, y), direction)) {
        return *to;
    }
    return nearest_that_way(state, rows, x, y, direction);
}

Target target_at(const A11yCssState& state, float x, float y) {
    if (state.hand.slider != A11Y_CSS_NO_SLIDER || state.slots[state.local_slot].name_tags_open) {
        return Target{};
    }
    if (state.hand.coin < 0) {
        Target target = first_at(state, top_bar_row(state), x, y);
        if (target.kind == TargetKind::none) {
            target = first_at(state,
                state.hand_count == A11Y_CSS_SLOTS ? slot_row(state) : arrows_in_test_order(state),
                x, y);
        }
        /* The name box after every slot, or after the arrows. */
        int own = state.local_slot;
        Target name_box{TargetKind::name_box, own};
        if (target.kind == TargetKind::none && own < state.slot_count &&
            state.slots[own].kind == A11Y_CSS_HUMAN && contains(state, name_box, x, y))
        {
            target = name_box;
        }
        if (target.kind != TargetKind::none) {
            return target;
        }
    }
    return first_at(state, unlocked_portraits(state), x, y);
}

}  // namespace a11y
