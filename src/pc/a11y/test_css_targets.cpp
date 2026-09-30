/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's targets against hand-written snapshots of the screen
 * (test_css_screen.hpp). */
#include "css_targets.hpp"
#include "test_css_screen.hpp"
#include <cassert>
#include <iostream>

namespace {

using a11y::Target;
using a11y::target_at;
using a11y::TargetKind;
using namespace css_test;

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
    std::cout << "css_targets: all tests passed\n";
    return 0;
}
