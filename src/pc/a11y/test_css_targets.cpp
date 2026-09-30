/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's targets and steps against hand-written snapshots of the
 * screen (test_css_screen.hpp). */
#include "css_targets.hpp"
#include "test_css_screen.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

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

/* A step from a portrait, with the hand where a glide to it ends. */
Target step_from(const A11yCssState& state, int from, Direction direction) {
    Point hand = a11y::aim_point(state, portrait(from));
    return a11y::step(state, portrait(from), hand.x, hand.y, direction);
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
 * the top two rows have columns 0 to 8, the bottom one 1 to 7. */
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

void single_player_modes_have_back_only_so_far() {
    A11yCssState state = vs_screen();
    state.hand_count = 1;
    state.has_teams_button = false;
    state.has_rules_button = false;
    assert((target_at(state, 20.0f, 24.0f) == Target{TargetKind::back}));
    assert((target_at(state, -32.0f, -2.0f) == Target{}));
    assert((target_at(state, -23.0f, 11.0f) == Target{TargetKind::portrait, kFoxPortrait}));
}

void aim_puts_the_coin_at_the_centre() {
    A11yCssState state = vs_screen();
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
            assert(step_from(state, from, direction) == portrait(expected_step(from, direction)));
        }
    }
}

void steps_stop_at_the_edges() {
    /* No wrapping: the step names the portrait the hand is on. Up from the
     * top row and Down from the bottom one are edges until the top bar and
     * the player slots have steps. */
    A11yCssState state = vs_screen();
    assert(step_from(state, kDrMarioPortrait, Direction::left) == portrait(kDrMarioPortrait));
    assert(step_from(state, kGanonPortrait, Direction::right) == portrait(kGanonPortrait));
    assert(step_from(state, kYoungLinkPortrait, Direction::right) == portrait(kYoungLinkPortrait));
    assert(step_from(state, kPichuPortrait, Direction::left) == portrait(kPichuPortrait));
    assert(step_from(state, kRoyPortrait, Direction::right) == portrait(kRoyPortrait));
    assert(step_from(state, kPeachPortrait, Direction::up) == portrait(kPeachPortrait));
    assert(step_from(state, kMewtwoPortrait, Direction::down) == portrait(kMewtwoPortrait));
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
    /* The hand's start, over player 1's slot: Up to the nearest row and in
     * it the portrait nearest in x, and Right along that row; nothing is
     * left or below. */
    assert(a11y::step(state, Target{}, -31.0f, -21.5f, Direction::up) == portrait(kPichuPortrait));
    assert(
        a11y::step(state, Target{}, -31.0f, -21.5f, Direction::right) == portrait(kPichuPortrait));
    assert(a11y::step(state, Target{}, -31.0f, -21.5f, Direction::left) == Target{});
    assert(a11y::step(state, Target{}, -31.0f, -21.5f, Direction::down) == Target{});
    /* Left of the bottom row, level with it: Right along the row, not up
     * to the nearer Falco. */
    Point pichu = a11y::aim_point(state, portrait(kPichuPortrait));
    assert(
        a11y::step(state, Target{}, -35.0f, pichu.y, Direction::right) == portrait(kPichuPortrait));
    /* Right of the bottom row's end, level with it: Left to Roy. */
    assert(a11y::step(state, Target{}, 25.0f, pichu.y, Direction::left) == portrait(kRoyPortrait));
    /* Nothing further right in the hand's row: the nearest that way in any
     * row. */
    Point roy = a11y::aim_point(state, portrait(kRoyPortrait));
    assert(a11y::step(state, Target{}, roy.x + 1.0f, pichu.y, Direction::right) ==
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
    single_player_modes_have_back_only_so_far();
    aim_puts_the_coin_at_the_centre();
    steps_from_every_portrait();
    steps_stop_at_the_edges();
    steps_between_rows_of_different_lengths();
    steps_skip_locked_portraits();
    a_row_with_every_portrait_locked_is_skipped();
    luigi_locked_trades_places_with_pikachu();
    a_press_during_a_glide_steps_on_from_its_destination();
    from_anywhere_else_the_nearest_that_way();
    std::cout << "css_targets: all tests passed\n";
    return 0;
}
