/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's targets and steps against hand-written snapshots of the
 * screen (test_css_screen.hpp). */
#include "css_targets.hpp"
#include "test_css_screen.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>
#include <vector>

namespace {

using a11y::Direction;
using a11y::Point;
using a11y::Target;
using a11y::target_at;
using a11y::TargetKind;
using namespace css_test;

constexpr Direction kDirections[] = {
    Direction::left, Direction::right, Direction::up, Direction::down};
constexpr int kDrMarioPortrait = 0, kMarioPortrait = 1, kLuigiPortrait = 2, kBowserPortrait = 3,
              kPeachPortrait = 4, kGanonPortrait = 8, kFalcoPortrait = 9, kKirbyPortrait = 13,
              kYoungLinkPortrait = 17, kPichuPortrait = 18, kPikachuPortrait = 19,
              kJigglypuffPortrait = 20, kMewtwoPortrait = 21, kGameWatchPortrait = 22,
              kRoyPortrait = 24;

Target portrait(int index) {
    return Target{TargetKind::portrait, index};
}

Target slot_button(int slot) {
    return Target{TargetKind::slot_button, slot};
}

Target team_button(int slot) {
    return Target{TargetKind::team_button, slot};
}

Target cpu_level(int slot, std::optional<int> level = std::nullopt) {
    return Target{TargetKind::cpu_level, slot, level};
}

Target handicap(int slot, std::optional<int> level = std::nullopt) {
    return Target{TargetKind::handicap, slot, level};
}

constexpr Target kTeams{TargetKind::teams};
constexpr Target kRules{TargetKind::rules};
constexpr Target kBack{TargetKind::back};
/* The arrows by their numbers in kArrows. */
constexpr Target kLower{TargetKind::arrow, 0};
constexpr Target kHigher{TargetKind::arrow, 1};
constexpr Target kFewer{TargetKind::arrow, 2};
constexpr Target kMore{TargetKind::arrow, 3};

using Row = std::vector<Target>;

/* A step from a target, with the hand where a glide to it ends. */
Target step_from(const A11yCssState& state, Target from, Direction direction) {
    Point hand = a11y::aim_point(state, from);
    return a11y::step(state, from, hand.x, hand.y, direction);
}

/* A step from a portrait, with the hand where a glide to it ends. */
Target step_from(const A11yCssState& state, int from, Direction direction) {
    return step_from(state, portrait(from), direction);
}

/* The portrait at a row and column as drawn (test_css_screen.hpp), or -1. */
int portrait_at(int row, int column) {
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        A11yCssRect rect = portrait_rect(i);
        if (rect.top == kRowTops[row] && rect.left == kColumnLefts[column]) {
            return i;
        }
    }
    return -1;
}

int row_of(int portrait) {
    return portrait < 9 ? 0 : portrait < 18 ? 1 : 2;
}

int column_of(int portrait) {
    return row_of(portrait) == 2 ? portrait - 18 + 1 : portrait - 9 * row_of(portrait);
}

/* Where a step goes with every portrait unlocked, worked out on the grid:
 * the top two rows have columns 0 to 8, the bottom one 1 to 7. Up from the
 * top row and Down from the bottom one leave the portraits, and are
 * reported here as staying. */
int expected_step(int from, Direction direction) {
    int row = row_of(from);
    int column = column_of(from);
    int to = -1;
    switch (direction) {
    case Direction::left:
        to = column > 0 ? portrait_at(row, column - 1) : -1;
        break;
    case Direction::right:
        to = column < 8 ? portrait_at(row, column + 1) : -1;
        break;
    case Direction::up:
        to = row > 0 ? portrait_at(row - 1, column) : -1;
        break;
    case Direction::down:
        to = row == 0 ? portrait_at(1, column) :
             row == 1 ? portrait_at(2, column < 1 ? 1 :
                                       column > 7 ? 7 :
                                                    column) :
                        -1;
        break;
    }
    return to >= 0 ? to : from;
}

void top_bar() {
    A11yCssState state = vs_screen();
    assert((target_at(state, 20.0f, 24.0f) == Target{TargetKind::back}));
    assert((target_at(state, 0.0f, 24.0f) == Target{TargetKind::rules}));
    assert((target_at(state, -30.0f, 24.0f) == Target{TargetKind::teams}));
    /* Between the buttons, and on their edges. */
    assert((target_at(state, -20.0f, 24.0f) == Target{}));
    assert((target_at(state, 16.0f, 24.0f) == Target{}));
    assert((target_at(state, 17.3f, 24.0f) == Target{}));
    assert((target_at(state, 0.0f, 22.0f) == Target{}));
}

void top_bar_without_teams_or_rules() {
    /* Stamina mode has no rules header; the special modes past Slo-Mo no
     * Teams button. */
    A11yCssState state = vs_screen();
    state.has_rules_button = false;
    state.has_teams_button = false;
    assert((target_at(state, 0.0f, 24.0f) == Target{}));
    assert((target_at(state, -30.0f, 24.0f) == Target{}));
    assert((target_at(state, 20.0f, 24.0f) == Target{TargetKind::back}));
}

void slot_buttons() {
    A11yCssState state = vs_screen();
    assert((target_at(state, -32.0f, -2.0f) == Target{TargetKind::slot_button, 0}));
    assert((target_at(state, -16.0f, -2.0f) == Target{TargetKind::slot_button, 1}));
    assert((target_at(state, -1.0f, -2.0f) == Target{TargetKind::slot_button, 2}));
    assert((target_at(state, 14.0f, -2.0f) == Target{TargetKind::slot_button, 3}));
    /* On an edge, below, and between two slots. */
    assert((target_at(state, -28.6f, -2.0f) == Target{}));
    assert((target_at(state, -32.0f, -4.6f) == Target{}));
    assert((target_at(state, -32.0f, -8.0f) == Target{}));
    assert((target_at(state, -25.0f, -2.0f) == Target{}));
}

void slot_button_waits_while_the_slot_is_in_a_hand() {
    A11yCssState state = vs_screen();
    state.slots[1].carried = true;
    assert((target_at(state, -16.0f, -2.0f) == Target{}));
    state = vs_screen();
    state.slots[1].hand_holding = true;
    assert((target_at(state, -16.0f, -2.0f) == Target{}));
    state = vs_screen();
    state.slots[1].cpu_level_held = true;
    assert((target_at(state, -16.0f, -2.0f) == Target{}));
    state = vs_screen();
    state.slots[1].name_tags_open = true;
    assert((target_at(state, -16.0f, -2.0f) == Target{}));
}

void camera_mode_leaves_out_the_fourth_slot() {
    A11yCssState state = vs_screen();
    state.slot_count = 3;
    assert((target_at(state, 14.0f, -2.0f) == Target{}));
    assert((target_at(state, -1.0f, -2.0f) == Target{TargetKind::slot_button, 2}));
}

void team_buttons_in_a_team_match_only() {
    A11yCssState state = vs_screen();
    assert((target_at(state, -24.0f, -3.0f) == Target{}));
    state.teams = true;
    assert((target_at(state, -24.0f, -3.0f) == Target{TargetKind::team_button, 0}));
    assert((target_at(state, -9.0f, -3.0f) == Target{TargetKind::team_button, 1}));
    /* A closed slot has none; nor has the edge. */
    assert((target_at(state, 6.0f, -3.0f) == Target{}));
    assert((target_at(state, -21.0f, -3.0f) == Target{}));
}

void cpu_level_knob_of_a_cpu_only() {
    A11yCssState state = vs_screen();
    /* Player 2 is a CPU: its knob, and within reach of it. */
    assert((target_at(state, -15.5f, -15.12f) == Target{TargetKind::cpu_level, 1}));
    assert((target_at(state, -14.0f, -14.0f) == Target{TargetKind::cpu_level, 1}));
    assert((target_at(state, -13.0f, -15.12f) == Target{}));
    /* Player 1 is human: its hidden knob is not a target. */
    assert((target_at(state, -30.9f, -20.12f) == Target{}));
    /* A knob another hand holds is taken. */
    state.slots[1].cpu_level_held = true;
    assert((target_at(state, -15.5f, -15.12f) == Target{}));
}

void cpu_level_knob_follows_its_level() {
    A11yCssState state = vs_screen();
    state.slots[1].cpu_level = 9;
    state.slots[1].cpu_level_knob = cpu_level_knob(1, 9);
    assert((target_at(state, -15.5f, -15.12f) == Target{}));
    assert((target_at(state, -5.5f, -15.12f) == Target{TargetKind::cpu_level, 1}));
}

void handicap_knobs_with_the_rule_on() {
    A11yCssState state = vs_screen();
    state.slots[0].handicap_knob = A11yCssKnob{true, -30.9f, -15.0f};
    state.slots[1].handicap_knob = A11yCssKnob{true, -15.5f, -12.0f};
    state.slots[2].handicap_knob = A11yCssKnob{true, -0.1f, -12.0f};
    assert((target_at(state, -30.9f, -15.0f) == Target{}));
    state.handicap_sliders = true;
    /* One's own, and a CPU's; not a closed slot's. */
    assert((target_at(state, -30.9f, -15.0f) == Target{TargetKind::handicap, 0}));
    assert((target_at(state, -15.5f, -12.0f) == Target{TargetKind::handicap, 1}));
    assert((target_at(state, -0.1f, -12.0f) == Target{}));
}

void handicap_knob_of_another_human_is_theirs() {
    A11yCssState state = vs_screen();
    state.handicap_sliders = true;
    state.slots[2].kind = A11Y_CSS_HUMAN;
    state.slots[2].handicap_knob = A11yCssKnob{true, -0.1f, -12.0f};
    assert((target_at(state, -0.1f, -12.0f) == Target{}));
}

void name_box_of_ones_own_human_slot() {
    A11yCssState state = vs_screen();
    assert((target_at(state, -28.0f, -18.5f) == Target{TargetKind::name_box, 0}));
    assert((target_at(state, -28.0f, -20.0f) == Target{}));
    state.slots[0].kind = A11Y_CSS_CPU;
    assert((target_at(state, -28.0f, -18.5f) == Target{}));
}

void portraits_are_tested_at_the_coin() {
    A11yCssState state = vs_screen();
    /* Fox's portrait spans x -24.4 to -17.4 and y 13 to 6; the coin sits
     * 2.7 right of and 2 below the hand. */
    assert((target_at(state, -23.0f, 11.0f) == Target{TargetKind::portrait, kFoxPortrait}));
    assert((target_at(state, -19.0f, 11.0f) == Target{TargetKind::portrait, kNessPortrait}));
    state.hand.coin = 0;
    assert((target_at(state, -23.0f, 11.0f) == Target{TargetKind::portrait, kFoxPortrait}));
    assert((target_at(state, -23.0f, 8.5f) == Target{TargetKind::portrait, kFoxPortrait}));
    /* The coin on the edges between two rows and between two columns. */
    assert((target_at(state, -23.0f, 8.0f) == Target{}));
    assert((target_at(state, -23.0f, 15.0f) == Target{}));
    assert((target_at(state, -27.1f, 11.0f) == Target{}));
}

void locked_portraits_are_not_targets() {
    A11yCssState state = vs_screen();
    state.portraits[kFoxPortrait].locked = true;
    assert((target_at(state, -23.0f, 11.0f) == Target{}));
}

void a_carrying_hand_presses_no_button() {
    A11yCssState state = vs_screen();
    state.hand.coin = 0;
    assert((target_at(state, 20.0f, 24.0f) == Target{}));
    assert((target_at(state, -16.0f, -2.0f) == Target{}));
    assert((target_at(state, -15.5f, -15.12f) == Target{}));
}

void nothing_while_holding_a_slider_or_naming() {
    A11yCssState state = vs_screen();
    state.hand.slider = A11Y_CSS_CPU_LEVEL;
    state.hand.slider_slot = 1;
    assert((target_at(state, -16.0f, -2.0f) == Target{}));
    state = vs_screen();
    state.slots[0].name_tags_open = true;
    assert((target_at(state, -16.0f, -2.0f) == Target{}));
}

void empty_space() {
    A11yCssState state = vs_screen();
    assert((target_at(state, -31.0f, -21.5f) == Target{}));
    assert((target_at(state, 25.0f, -10.0f) == Target{}));
}

void single_player_top_bar_is_back_alone() {
    A11yCssState state = classic_screen();
    assert(target_at(state, 20.0f, 24.0f) == kBack);
    assert((target_at(state, -30.0f, 24.0f) == Target{}));
    assert((target_at(state, 0.0f, 24.0f) == Target{}));
    assert((target_at(state, -32.0f, -2.0f) == Target{}));
    assert(target_at(state, -23.0f, 11.0f) == portrait(kFoxPortrait));
}

void single_player_arrows() {
    /* Inside each arrow's rectangle, strictly; nothing between them. */
    A11yCssState state = classic_screen();
    assert(target_at(state, -7.5f, -9.0f) == kLower);
    assert(target_at(state, 16.0f, -9.0f) == kHigher);
    assert(target_at(state, 0.7f, -14.5f) == kFewer);
    assert(target_at(state, 16.5f, -14.5f) == kMore);
    assert((target_at(state, -7.5f, -12.1f) == Target{}));
    assert((target_at(state, 5.0f, -14.5f) == Target{}));
    assert((target_at(state, 5.0f, -9.0f) == Target{}));
    assert((target_at(state, 16.5f, -12.2f) == Target{}));
}

void single_player_arrows_by_mode() {
    /* All-Star has no stock arrows; the Stadium modes, Event Match and
     * Training have no arrows. */
    A11yCssState state = classic_screen();
    state.match_type = A11Y_REG_ALLSTAR;
    state.arrows.stocks.shown = false;
    assert(target_at(state, -7.5f, -9.0f) == kLower);
    assert((target_at(state, 0.7f, -14.5f) == Target{}));
    state = training_screen();
    assert((target_at(state, -7.5f, -9.0f) == Target{}));
    assert((target_at(state, 16.5f, -14.5f) == Target{}));
}

void single_player_arrows_wait_while_carrying_a_coin() {
    A11yCssState state = classic_screen();
    state.hand.coin = 0;
    assert((target_at(state, -7.5f, -9.0f) == Target{}));
    assert((target_at(state, 16.5f, -14.5f) == Target{}));
}

void single_player_name_box() {
    A11yCssState state = classic_screen();
    assert((target_at(state, -28.0f, -18.5f) == Target{TargetKind::name_box, 0}));
}

void arrows_at_the_ends_of_their_ranges() {
    A11yCssState state = classic_screen();
    for (Target arrow : {kLower, kHigher, kFewer, kMore}) {
        assert(!a11y::arrow_at_end(state, arrow));
    }
    state.arrows.difficulty.value = 0;
    state.arrows.stocks.value = 1;
    assert(a11y::arrow_at_end(state, kLower) && !a11y::arrow_at_end(state, kHigher));
    assert(a11y::arrow_at_end(state, kFewer) && !a11y::arrow_at_end(state, kMore));
    state.arrows.difficulty.value = 4;
    state.arrows.stocks.value = 5;
    assert(!a11y::arrow_at_end(state, kLower) && a11y::arrow_at_end(state, kHigher));
    assert(!a11y::arrow_at_end(state, kFewer) && a11y::arrow_at_end(state, kMore));
}

void aim_puts_the_coin_at_the_centre() {
    /* Carrying a coin; a free hand aims there too, unless a coin rests on
     * the portrait for it to pick up (player 2's on Yoshi). */
    A11yCssState state = vs_screen();
    state.hand.coin = 0;
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        Point hand = a11y::aim_point(state, portrait(i));
        A11yCssRect rect = portrait_rect(i);
        assert(std::fabs(hand.x + a11y::kCoinOffsetX - (rect.left + rect.right) / 2.0f) < 1e-4f);
        assert(std::fabs(hand.y + a11y::kCoinOffsetY - (rect.top + rect.bottom) / 2.0f) < 1e-4f);
        assert(target_at(state, hand.x, hand.y) == portrait(i));
    }
}

void steps_from_every_portrait() {
    A11yCssState state = vs_screen();
    for (int from = 0; from < A11Y_CSS_PORTRAITS; from++) {
        for (Direction direction : kDirections) {
            bool leaves = (direction == Direction::up && row_of(from) == 0) ||
                          (direction == Direction::down && row_of(from) == 2);
            if (!leaves) {
                assert(
                    step_from(state, from, direction) == portrait(expected_step(from, direction)));
            }
        }
    }
}

void steps_stop_at_the_edges() {
    /* No wrapping: the step names the target the hand is on. */
    A11yCssState state = vs_screen();
    assert(step_from(state, kDrMarioPortrait, Direction::left) == portrait(kDrMarioPortrait));
    assert(step_from(state, kGanonPortrait, Direction::right) == portrait(kGanonPortrait));
    assert(step_from(state, kYoungLinkPortrait, Direction::right) == portrait(kYoungLinkPortrait));
    assert(step_from(state, kPichuPortrait, Direction::left) == portrait(kPichuPortrait));
    assert(step_from(state, kRoyPortrait, Direction::right) == portrait(kRoyPortrait));
    assert(step_from(state, kTeams, Direction::left) == kTeams);
    assert(step_from(state, kBack, Direction::right) == kBack);
    assert(step_from(state, kRules, Direction::up) == kRules);
    assert(step_from(state, slot_button(0), Direction::left) == slot_button(0));
    assert(step_from(state, slot_button(3), Direction::right) == slot_button(3));
    assert(step_from(state, slot_button(2), Direction::down) == slot_button(2));
    assert(step_from(state, cpu_level(1), Direction::down) == cpu_level(1));
}

void steps_along_the_top_bar_and_the_player_slots() {
    A11yCssState state = vs_screen();
    assert(step_from(state, kTeams, Direction::right) == kRules);
    assert(step_from(state, kRules, Direction::right) == kBack);
    assert(step_from(state, kBack, Direction::left) == kRules);
    /* Player 2 is a CPU: its button, then its CPU level knob. */
    assert(step_from(state, slot_button(0), Direction::right) == slot_button(1));
    assert(step_from(state, slot_button(1), Direction::right) == cpu_level(1));
    assert(step_from(state, cpu_level(1), Direction::right) == slot_button(2));
    assert(step_from(state, slot_button(2), Direction::left) == cpu_level(1));
}

void up_and_down_between_rows_of_different_lengths() {
    /* The top bar's three buttons over the top row's nine portraits; the
     * bottom row's seven over the player slots' five targets. From each end
     * and the middle, to the target nearest in x. */
    A11yCssState state = vs_screen();
    assert(step_from(state, kDrMarioPortrait, Direction::up) == kTeams);
    assert(step_from(state, kPeachPortrait, Direction::up) == kRules);
    assert(step_from(state, kGanonPortrait, Direction::up) == kBack);
    assert(step_from(state, kTeams, Direction::down) == portrait(kDrMarioPortrait));
    assert(step_from(state, kRules, Direction::down) == portrait(kPeachPortrait));
    assert(step_from(state, kBack, Direction::down) == portrait(kGanonPortrait));
    assert(step_from(state, kPichuPortrait, Direction::down) == slot_button(1));
    assert(step_from(state, kJigglypuffPortrait, Direction::down) == cpu_level(1));
    assert(step_from(state, kMewtwoPortrait, Direction::down) == slot_button(2));
    assert(step_from(state, kRoyPortrait, Direction::down) == slot_button(3));
    assert(step_from(state, slot_button(0), Direction::up) == portrait(kPichuPortrait));
    assert(step_from(state, cpu_level(1), Direction::up) == portrait(kPikachuPortrait));
    assert(step_from(state, slot_button(2), Direction::up) == portrait(kMewtwoPortrait));
    assert(step_from(state, slot_button(3), Direction::up) == portrait(kMarthPortrait));
}

void rows_in_a_plain_vs_match() {
    A11yCssState state = vs_screen();
    std::vector<Row> rows = a11y::target_rows(state);
    assert(rows.size() == 5);
    assert((rows[0] == Row{kTeams, kRules, kBack}));
    assert(rows[1].size() == 9 && rows[2].size() == 9 && rows[3].size() == 7);
    assert(rows[1].front() == portrait(kDrMarioPortrait));
    assert(rows[3].back() == portrait(kRoyPortrait));
    assert((rows[4] ==
            Row{slot_button(0), slot_button(1), cpu_level(1), slot_button(2), slot_button(3)}));
}

void rows_in_a_team_match() {
    /* A team button for each open slot, after its HMN/CPU button. */
    A11yCssState state = vs_screen();
    state.teams = true;
    std::vector<Row> rows = a11y::target_rows(state);
    assert((rows.back() == Row{slot_button(0), team_button(0), slot_button(1), team_button(1),
                               cpu_level(1), slot_button(2), slot_button(3)}));
}

void rows_with_the_handicap_rule_on() {
    /* One's own handicap and a CPU's; not a closed slot's. */
    A11yCssState state = vs_screen();
    state.handicap_sliders = true;
    std::vector<Row> rows = a11y::target_rows(state);
    assert((rows.back() == Row{slot_button(0), handicap(0), slot_button(1), cpu_level(1),
                               handicap(1), slot_button(2), slot_button(3)}));
}

void rows_in_stamina_and_camera_modes() {
    /* Stamina mode has no rules header; Camera mode's fourth slot is the
     * camera. */
    A11yCssState state = vs_screen();
    state.has_rules_button = false;
    state.slot_count = 3;
    std::vector<Row> rows = a11y::target_rows(state);
    assert((rows.front() == Row{kTeams, kBack}));
    assert((rows.back() == Row{slot_button(0), slot_button(1), cpu_level(1), slot_button(2)}));
    assert(step_from(state, kTeams, Direction::right) == kBack);
}

void rows_in_classic() {
    /* Back alone over the portraits; the difficulty arrows above the stock
     * arrows below them, as drawn. */
    A11yCssState state = classic_screen();
    std::vector<Row> rows = a11y::target_rows(state);
    assert(rows.size() == 6);
    assert((rows[0] == Row{kBack}));
    assert(rows[1].size() == 9 && rows[2].size() == 9 && rows[3].size() == 7);
    assert((rows[4] == Row{kLower, kHigher}));
    assert((rows[5] == Row{kFewer, kMore}));
}

void rows_in_all_star_and_training() {
    A11yCssState state = classic_screen();
    state.match_type = A11Y_REG_ALLSTAR;
    state.arrows.stocks.shown = false;
    std::vector<Row> rows = a11y::target_rows(state);
    assert(rows.size() == 5);
    assert((rows.back() == Row{kLower, kHigher}));
    /* Training's two slots have no buttons: the portraits are the last row. */
    rows = a11y::target_rows(training_screen());
    assert(rows.size() == 4);
    assert((rows[0] == Row{kBack}));
    assert(rows.back().back() == portrait(kRoyPortrait));
}

void steps_over_the_arrows() {
    A11yCssState state = classic_screen();
    assert(step_from(state, kLower, Direction::right) == kHigher);
    assert(step_from(state, kHigher, Direction::left) == kLower);
    assert(step_from(state, kFewer, Direction::right) == kMore);
    assert(step_from(state, kMore, Direction::left) == kFewer);
    /* Each arrow's row to the one nearest in x in the next. */
    assert(step_from(state, kLower, Direction::down) == kFewer);
    assert(step_from(state, kHigher, Direction::down) == kMore);
    assert(step_from(state, kFewer, Direction::up) == kLower);
    assert(step_from(state, kMore, Direction::up) == kHigher);
    assert(step_from(state, kLower, Direction::up) == portrait(kJigglypuffPortrait));
    assert(step_from(state, kHigher, Direction::up) == portrait(kRoyPortrait));
    assert(step_from(state, kPichuPortrait, Direction::down) == kLower);
    assert(step_from(state, kRoyPortrait, Direction::down) == kHigher);
    /* The edges. */
    assert(step_from(state, kLower, Direction::left) == kLower);
    assert(step_from(state, kHigher, Direction::right) == kHigher);
    assert(step_from(state, kFewer, Direction::down) == kFewer);
    assert(step_from(state, kMore, Direction::right) == kMore);
    /* From the top bar, Back down to the portrait nearest in x. */
    assert(step_from(state, kBack, Direction::down) == portrait(kGanonPortrait));
    assert(step_from(state, kDrMarioPortrait, Direction::up) == kBack);
}

void single_player_steps_from_the_hands_start() {
    /* The hand starts below the player slot, left of the arrows. */
    A11yCssState state = classic_screen();
    float x = state.hand.x;
    float y = state.hand.y;
    assert(a11y::step(state, Target{}, x, y, Direction::up) == kFewer);
    assert(a11y::step(state, Target{}, x, y, Direction::right) == kFewer);
    assert((a11y::step(state, Target{}, x, y, Direction::left) == Target{}));
    state = training_screen();
    assert(a11y::step(state, Target{}, x, y, Direction::up) == portrait(kPichuPortrait));
    assert(step_from(state, kPichuPortrait, Direction::down) == portrait(kPichuPortrait));
}

void every_single_player_target_is_where_its_glide_ends() {
    /* A free hand at a target's aim is on it, for hover and for locate. */
    for (A11yCssState state : {classic_screen(), training_screen()}) {
        for (const Row& row : a11y::target_rows(state)) {
            for (Target target : row) {
                Point hand = a11y::aim_point(state, target);
                assert(a11y::locate(state, hand.x, hand.y) == target);
                if (target.kind != TargetKind::portrait) {
                    assert(target_at(state, hand.x, hand.y) == target);
                }
            }
        }
    }
}

void training_cpus_coin_is_pickable() {
    /* The CPU's coin rests on Dr. Mario; a free hand aims where A picks it
     * up. */
    A11yCssState state = training_screen();
    assert(a11y::pickable_coin(state, kDrMarioPortrait) == 1);
    assert(a11y::pickable_coin(state, kMarioPortrait) == -1);
    Point aim = a11y::aim_point(state, portrait(kDrMarioPortrait));
    assert(std::fabs(aim.x - (state.slots[1].coin.x - 3.8f)) < 1e-4f);
}

void rows_are_rebuilt_as_a_slot_changes() {
    /* The hand on player 3's button makes it a CPU: its knob follows. */
    A11yCssState state = vs_screen();
    state.slots[2] = cpu(2, kFoxPortrait);
    assert(step_from(state, slot_button(2), Direction::right) == cpu_level(2));
    /* Player 2 closes while the hand is on its CPU level knob, now hidden:
     * a step goes from where the hand is. */
    state = vs_screen();
    Point knob = a11y::aim_point(state, cpu_level(1));
    state.slots[1].kind = A11Y_CSS_CLOSED;
    assert(a11y::step(state, cpu_level(1), knob.x, knob.y, Direction::right) == slot_button(2));
    assert(a11y::step(state, cpu_level(1), knob.x, knob.y, Direction::left) == slot_button(1));
    assert(a11y::step(state, cpu_level(1), knob.x, knob.y, Direction::up) == slot_button(1));
}

void a_held_slider_steps_its_value() {
    A11yCssState state = vs_screen();
    state.hand.slider = A11Y_CSS_CPU_LEVEL;
    state.hand.slider_slot = 1;
    state.slots[1].cpu_level_held = true;
    const A11yCssKnob& knob = state.slots[1].cpu_level_knob;
    /* From 1 to 9 and back, each step from the value the last went to, as
     * steps during a glide do; the aim is inside the value's span. */
    Target at = a11y::locate(state, knob.x, knob.y);
    assert(at == cpu_level(1, 1));
    for (int level = 2; level <= 9; level++) {
        at = step_from(state, at, Direction::right);
        assert(at == cpu_level(1, level));
        assert(slider_value(knob.origin_x, a11y::aim_point(state, at).x) == level);
        assert(std::fabs(a11y::aim_point(state, at).y - knob.y) < 1e-4f);
    }
    assert(step_from(state, at, Direction::right) == cpu_level(1, 9));
    for (int level = 8; level >= 1; level--) {
        at = step_from(state, at, Direction::left);
        assert(at == cpu_level(1, level));
        assert(slider_value(knob.origin_x, a11y::aim_point(state, at).x) == level);
    }
    assert(step_from(state, at, Direction::left) == cpu_level(1, 1));
    /* Up and Down keep the value. */
    state.slots[1].cpu_level = 4;
    Point hand = a11y::aim_point(state, cpu_level(1, 4));
    assert(a11y::step(state, Target{}, hand.x, hand.y, Direction::up) == cpu_level(1, 4));
    assert(a11y::step(state, Target{}, hand.x, hand.y, Direction::down) == cpu_level(1, 4));
    assert(a11y::step(state, Target{}, hand.x, hand.y, Direction::right) == cpu_level(1, 5));
}

void a_held_handicap_slider_steps_too() {
    A11yCssState state = vs_screen();
    state.handicap_sliders = true;
    state.hand.slider = A11Y_CSS_HANDICAP;
    state.hand.slider_slot = 0;
    assert(a11y::step(state, Target{}, 0.0f, 0.0f, Direction::left) == handicap(0, 8));
    assert(a11y::step(state, Target{}, 0.0f, 0.0f, Direction::right) == handicap(0, 9));
}

void locate_finds_what_does_not_react_now() {
    A11yCssState state = vs_screen();
    /* A carried coin: the top bar and the portraits. */
    state.hand.coin = 0;
    assert(a11y::locate(state, 20.0f, 24.0f) == kBack);
    assert(a11y::locate(state, -23.0f, 11.0f) == portrait(kFoxPortrait));
    /* Player 2's button while its coin is in another hand. */
    state = vs_screen();
    state.slots[1].carried = true;
    assert(a11y::locate(state, -16.0f, -2.0f) == slot_button(1));
    assert(target_at(state, -16.0f, -2.0f) == Target{});
    /* A knob, and empty space; the name box is not among the rows. */
    assert(a11y::locate(state, -15.0f, -15.0f) == cpu_level(1));
    assert(a11y::locate(state, -25.0f, -2.0f) == Target{});
    assert(a11y::locate(state, -28.0f, -18.5f) == Target{});
}

void pickable_coins() {
    A11yCssState state = vs_screen();
    /* Player 2 is a CPU with Yoshi; player 1's own coin rests on Fox. */
    state.slots[0].portrait = kFoxPortrait;
    state.slots[0].character = kFox;
    state.slots[0].coin = coin_at_rest(kFoxPortrait);
    assert(a11y::pickable_coin(state, kYoshiPortrait) == 1);
    assert(a11y::pickable_coin(state, kFoxPortrait) == 0);
    assert(a11y::pickable_coin(state, kNessPortrait) == -1);
    /* Two on one portrait: the CPU's. */
    state.slots[0].portrait = kYoshiPortrait;
    assert(a11y::pickable_coin(state, kYoshiPortrait) == 1);
    /* Another human's coin is theirs. */
    state.slots[1].kind = A11Y_CSS_HUMAN;
    assert(a11y::pickable_coin(state, kYoshiPortrait) == 0);
    /* A hand carrying a coin, or holding a slider, picks up none. */
    state = vs_screen();
    state.hand.coin = 0;
    assert(a11y::pickable_coin(state, kYoshiPortrait) == -1);
    state = vs_screen();
    state.hand.slider = A11Y_CSS_CPU_LEVEL;
    state.hand.slider_slot = 1;
    assert(a11y::pickable_coin(state, kYoshiPortrait) == -1);
    /* A closed slot's coin is not shown; a coin in another hand is taken. */
    state = vs_screen();
    state.slots[1].kind = A11Y_CSS_CLOSED;
    assert(a11y::pickable_coin(state, kYoshiPortrait) == -1);
    state = vs_screen();
    state.slots[1].carried = true;
    assert(a11y::pickable_coin(state, kYoshiPortrait) == -1);
}

/* A at the hand picks up the coin of a slot (mnCharSel_CursorThink): within
 * 3 units of the hand plus (3.8, -2.6), the hand in the portrait band. */
bool picks_up(const A11yCssState& state, Point hand, int slot) {
    float dx = hand.x + 3.8f - state.slots[slot].coin.x;
    float dy = hand.y - 2.6f - state.slots[slot].coin.y;
    return dx * dx + dy * dy < 9.0f && hand.y > 0.2f && hand.y < 22.0f;
}

void a_step_to_a_coin_aims_where_a_picks_it_up() {
    A11yCssState state = vs_screen();
    Point hand = a11y::aim_point(state, portrait(kYoshiPortrait));
    assert(picks_up(state, hand, 1));
    /* A coin drifted to the very top of its portrait: still in the band. */
    state.slots[1].coin.y = portrait_rect(kYoshiPortrait).top;
    hand = a11y::aim_point(state, portrait(kYoshiPortrait));
    assert(picks_up(state, hand, 1));
    /* Carrying a coin, the portrait's centre. */
    state.hand.coin = 0;
    hand = a11y::aim_point(state, portrait(kYoshiPortrait));
    A11yCssRect rect = portrait_rect(kYoshiPortrait);
    assert(std::fabs(hand.x + a11y::kCoinOffsetX - (rect.left + rect.right) / 2.0f) < 1e-4f);
}

void steps_between_rows_of_different_lengths() {
    /* The bottom row starts one column in and ends one early. */
    A11yCssState state = vs_screen();
    assert(step_from(state, kFalcoPortrait, Direction::down) == portrait(kPichuPortrait));
    assert(step_from(state, kYoungLinkPortrait, Direction::down) == portrait(kRoyPortrait));
    assert(step_from(state, kPichuPortrait, Direction::up) == portrait(kFoxPortrait));
}

void steps_skip_locked_portraits() {
    A11yCssState state = vs_screen();
    state.portraits[kFoxPortrait].locked = true;
    state.portraits[kMewtwoPortrait].locked = true;
    assert(step_from(state, kFalcoPortrait, Direction::right) == portrait(kNessPortrait));
    assert(step_from(state, kNessPortrait, Direction::left) == portrait(kFalcoPortrait));
    assert(step_from(state, kJigglypuffPortrait, Direction::right) == portrait(kGameWatchPortrait));
    /* Nearest in x: Falco and Ness are as near to Fox's column; the left
     * one wins. */
    assert(step_from(state, kPichuPortrait, Direction::up) == portrait(kFalcoPortrait));
    assert(step_from(state, kMarioPortrait, Direction::down) == portrait(kFalcoPortrait));
    assert(step_from(state, kPeachPortrait, Direction::down) == portrait(kKirbyPortrait));
    assert(step_from(state, kKirbyPortrait, Direction::down) == portrait(kJigglypuffPortrait));
}

void a_row_with_every_portrait_locked_is_skipped() {
    A11yCssState state = vs_screen();
    for (int i = kFalcoPortrait; i <= kYoungLinkPortrait; i++) {
        state.portraits[i].locked = true;
    }
    assert(step_from(state, kBowserPortrait, Direction::down) == portrait(kJigglypuffPortrait));
    assert(step_from(state, kJigglypuffPortrait, Direction::up) == portrait(kBowserPortrait));
}

void luigi_locked_trades_places_with_pikachu() {
    /* With Luigi locked, mnCharSel_802640A0 builds the screen with Luigi's
     * portrait in Pikachu's place on the bottom row and Pikachu's on the
     * top row. */
    A11yCssState state = vs_screen();
    state.portraits[kLuigiPortrait].rect = portrait_rect(kPikachuPortrait);
    state.portraits[kPikachuPortrait].rect = portrait_rect(kLuigiPortrait);
    state.portraits[kLuigiPortrait].locked = true;
    assert(step_from(state, kMarioPortrait, Direction::right) == portrait(kPikachuPortrait));
    assert(step_from(state, kPikachuPortrait, Direction::right) == portrait(kBowserPortrait));
    assert(step_from(state, kPikachuPortrait, Direction::left) == portrait(kMarioPortrait));
    assert(step_from(state, kPikachuPortrait, Direction::down) == portrait(kNessPortrait));
    assert(step_from(state, kNessPortrait, Direction::up) == portrait(kPikachuPortrait));
    assert(step_from(state, kPichuPortrait, Direction::right) == portrait(kJigglypuffPortrait));
    assert(step_from(state, kNessPortrait, Direction::down) == portrait(kPichuPortrait));
}

void a_press_during_a_glide_steps_on_from_its_destination() {
    /* The hand is still over Dr. Mario, gliding to Mario. */
    A11yCssState state = vs_screen();
    Point hand = a11y::aim_point(state, portrait(kDrMarioPortrait));
    assert(a11y::step(state, portrait(kMarioPortrait), hand.x, hand.y, Direction::right) ==
           portrait(kLuigiPortrait));
}

void from_anywhere_else_the_nearest_that_way() {
    A11yCssState state = vs_screen();
    /* The hand's start, over player 1's slot, below every button: Up to the
     * player slots' row and in it the target nearest in x, Left and Right
     * along it; nothing is below. */
    assert(a11y::step(state, Target{}, -31.0f, -21.5f, Direction::up) == slot_button(0));
    assert(a11y::step(state, Target{}, -31.0f, -21.5f, Direction::left) == slot_button(0));
    assert(a11y::step(state, Target{}, -31.0f, -21.5f, Direction::right) == slot_button(1));
    assert(a11y::step(state, Target{}, -31.0f, -21.5f, Direction::down) == Target{});
    /* Between the buttons and the knobs: Down to the knob, Up to the
     * button. */
    assert(a11y::step(state, Target{}, -15.0f, -9.0f, Direction::down) == cpu_level(1));
    assert(a11y::step(state, Target{}, -15.0f, -9.0f, Direction::up) == slot_button(1));
    /* Left of the bottom row, level with it: Right along the row, not up
     * to the nearer Falco. */
    Point pichu = a11y::aim_point(state, portrait(kPichuPortrait));
    assert(
        a11y::step(state, Target{}, -35.0f, pichu.y, Direction::right) == portrait(kPichuPortrait));
    /* Right of the bottom row's end, level with it: Left to Roy. */
    assert(a11y::step(state, Target{}, 25.0f, pichu.y, Direction::left) == portrait(kRoyPortrait));
    /* Nothing further right in the hand's row: the nearest that way in any
     * row. Just right of where the glide to Roy ends, the hand is still on
     * Roy, the row's end. */
    Point roy = a11y::aim_point(state, portrait(kRoyPortrait));
    assert(a11y::step(state, Target{}, roy.x + 1.0f, pichu.y, Direction::right) ==
           portrait(kRoyPortrait));
    assert(a11y::step(state, Target{}, roy.x + 5.0f, pichu.y, Direction::right) ==
           portrait(kYoungLinkPortrait));
    /* Over the rules header, Down goes to the top row below it. */
    assert(a11y::step(state, Target{TargetKind::rules}, 0.0f, 24.0f, Direction::down) ==
           portrait(kPeachPortrait));
    /* A locked portrait is no target: the hand over it is on nothing. */
    state.portraits[kFoxPortrait].locked = true;
    Point fox = a11y::aim_point(state, portrait(kFoxPortrait));
    assert(target_at(state, fox.x, fox.y) == Target{});
    assert(a11y::step(state, Target{}, fox.x, fox.y, Direction::right) == portrait(kNessPortrait));
}

}  // namespace

int main() {
    top_bar();
    top_bar_without_teams_or_rules();
    slot_buttons();
    slot_button_waits_while_the_slot_is_in_a_hand();
    camera_mode_leaves_out_the_fourth_slot();
    team_buttons_in_a_team_match_only();
    cpu_level_knob_of_a_cpu_only();
    cpu_level_knob_follows_its_level();
    handicap_knobs_with_the_rule_on();
    handicap_knob_of_another_human_is_theirs();
    name_box_of_ones_own_human_slot();
    portraits_are_tested_at_the_coin();
    locked_portraits_are_not_targets();
    a_carrying_hand_presses_no_button();
    nothing_while_holding_a_slider_or_naming();
    empty_space();
    single_player_top_bar_is_back_alone();
    single_player_arrows();
    single_player_arrows_by_mode();
    single_player_arrows_wait_while_carrying_a_coin();
    single_player_name_box();
    arrows_at_the_ends_of_their_ranges();
    aim_puts_the_coin_at_the_centre();
    steps_from_every_portrait();
    steps_stop_at_the_edges();
    steps_along_the_top_bar_and_the_player_slots();
    up_and_down_between_rows_of_different_lengths();
    rows_in_a_plain_vs_match();
    rows_in_a_team_match();
    rows_with_the_handicap_rule_on();
    rows_in_stamina_and_camera_modes();
    rows_in_classic();
    rows_in_all_star_and_training();
    steps_over_the_arrows();
    single_player_steps_from_the_hands_start();
    every_single_player_target_is_where_its_glide_ends();
    training_cpus_coin_is_pickable();
    rows_are_rebuilt_as_a_slot_changes();
    a_held_slider_steps_its_value();
    a_held_handicap_slider_steps_too();
    locate_finds_what_does_not_react_now();
    pickable_coins();
    a_step_to_a_coin_aims_where_a_picks_it_up();
    steps_between_rows_of_different_lengths();
    steps_skip_locked_portraits();
    a_row_with_every_portrait_locked_is_skipped();
    luigi_locked_trades_places_with_pikachu();
    a_press_during_a_glide_steps_on_from_its_destination();
    from_anywhere_else_the_nearest_that_way();
    std::cout << "css_targets: all tests passed\n";
    return 0;
}
