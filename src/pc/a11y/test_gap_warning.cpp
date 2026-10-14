/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The gap warning's driver against hand-written match snapshots on Final
 * Destination's frame as measured (docs/a11y/gameplay-state.md, "Sizes
 * measured"): on the ground, in the air with the main floor or a platform
 * below, over a gap, the hold on leaving a gap, and silent while knocked
 * out, paused, before GO and outside a match. */
#include "gap_warning.hpp"
#include "test_match_snapshot.hpp"
#include <cassert>
#include <iostream>

namespace {

using namespace match_test;
using a11y::GapMapping;
using a11y::GapWarning;

/* Line ids as on Final Destination and Battlefield: the main floor's
 * middle piece, and a platform. */
constexpr int kMainFloor = 1;
constexpr int kPlatform = 20;

/* You at (x, y) on Final Destination, the opponent at the centre, with no
 * floor below you until one is given. */
A11yMatchState match_at(float x, float y, bool airborne) {
    A11yMatchState match{};
    match.running = true;
    match.you = fighter(0, x, y);
    match.you.airborne = airborne;
    match.below.line = -1;
    match.opponent_count = 1;
    match.opponents[0] = fighter(1, 0.0f, 0.0f);
    match.stage = final_destination();
    return match;
}

A11yMatchState with_floor(A11yMatchState match, int line, float y) {
    match.below.found = true;
    match.below.line = line;
    match.below.y = y;
    return match;
}

A11yMatchState standing(float x) {
    return with_floor(match_at(x, 0.0f, false), kMainFloor, 0.0f);
}

/* Past the left edge, falling. */
A11yMatchState over_the_gap() {
    return match_at(-100.0f, -20.0f, true);
}

void ground_is_no_gap() {
    assert(!a11y::over_gap(standing(0.0f)));
    assert(!a11y::over_gap(standing(-85.0f)));
    /* On the ground the cast is not made; even a snapshot with no floor
     * below is no gap. */
    assert(!a11y::over_gap(match_at(-85.0f, 0.0f, false)));
    GapWarning gap;
    assert(!gap.tick(standing(-85.0f), GapMapping{}).has_value());
}

void air_with_floor_below_is_no_gap() {
    /* Jumping above the main floor, near its edge. */
    A11yMatchState jump = with_floor(match_at(-84.0f, 30.0f, true), kMainFloor, 0.0f);
    assert(!a11y::over_gap(jump));
    GapWarning gap;
    assert(!gap.tick(jump, GapMapping{}).has_value());
}

void air_with_a_platform_below_is_no_gap() {
    /* Above Battlefield's left platform, high up. */
    A11yMatchState above = with_floor(match_at(-40.0f, 60.0f, true), kPlatform, 27.2f);
    assert(!a11y::over_gap(above));
    GapWarning gap;
    assert(!gap.tick(above, GapMapping{}).has_value());
}

void a_gap_sounds_centred_and_level() {
    assert(a11y::over_gap(over_the_gap()));
    GapWarning gap;
    auto targets = gap.tick(over_the_gap(), GapMapping{});
    assert(targets.has_value());
    assert(targets->pan == 0.0f && targets->pitch == 1.0f && targets->gain == 1.0f);
    /* The same however far out or down. */
    A11yMatchState deeper = match_at(-200.0f, -120.0f, true);
    auto deep = gap.tick(deeper, GapMapping{});
    assert(deep.has_value());
    assert(deep->pan == 0.0f && deep->pitch == 1.0f && deep->gain == 1.0f);
}

void the_hold_on_leaving_a_gap() {
    GapWarning gap;
    const GapMapping mapping{3};
    assert(gap.tick(over_the_gap(), mapping).has_value());
    /* Back over the floor: three more ticks, then silence. */
    A11yMatchState back = with_floor(match_at(-84.0f, -5.0f, true), kMainFloor, 0.0f);
    assert(gap.tick(back, mapping).has_value());
    assert(gap.tick(back, mapping).has_value());
    assert(gap.tick(back, mapping).has_value());
    assert(!gap.tick(back, mapping).has_value());
    assert(!gap.tick(back, mapping).has_value());
    /* A gap within the hold starts it afresh. */
    assert(gap.tick(over_the_gap(), mapping).has_value());
    assert(gap.tick(back, mapping).has_value());
    assert(gap.tick(over_the_gap(), mapping).has_value());
    assert(gap.tick(standing(-80.0f), mapping).has_value());
    assert(gap.tick(standing(-80.0f), mapping).has_value());
    assert(gap.tick(standing(-80.0f), mapping).has_value());
    assert(!gap.tick(standing(-80.0f), mapping).has_value());
    /* No hold: silent on the first tick off the gap. */
    GapWarning sharp;
    assert(sharp.tick(over_the_gap(), GapMapping{0}).has_value());
    assert(!sharp.tick(back, GapMapping{0}).has_value());
}

void knocked_out_is_silent_at_once() {
    GapWarning gap;
    assert(gap.tick(over_the_gap(), GapMapping{}).has_value());
    /* Past the bottom blast zone: out, the hold or not. */
    A11yMatchState dead = match_at(-100.0f, -150.0f, true);
    dead.you.out = true;
    assert(!gap.tick(dead, GapMapping{}).has_value());
    /* Waiting to respawn, then on the respawn platform with the floor
     * below: still silent, the hold spent. */
    assert(!gap.tick(dead, GapMapping{}).has_value());
    A11yMatchState respawned = with_floor(match_at(16.0f, 112.9f, true), kMainFloor, 0.0f);
    assert(!gap.tick(respawned, GapMapping{}).has_value());
}

void silent_while_paused_before_go_and_outside_a_match() {
    GapWarning gap;
    A11yMatchState paused = over_the_gap();
    paused.paused = true;
    assert(!gap.tick(paused, GapMapping{}).has_value());
    A11yMatchState before_go = over_the_gap();
    before_go.you.can_move = false;
    assert(!gap.tick(before_go, GapMapping{}).has_value());
    A11yMatchState none{};
    none.below.line = -1;
    assert(!a11y::over_gap(none));
    assert(!gap.tick(none, GapMapping{}).has_value());
    /* A pause cuts the hold too: back from it, only a gap sounds. */
    assert(gap.tick(over_the_gap(), GapMapping{}).has_value());
    assert(!gap.tick(paused, GapMapping{}).has_value());
    assert(!gap.tick(standing(-80.0f), GapMapping{}).has_value());
    /* Slow motion's skipped ticks are no pause. */
    A11yMatchState held = over_the_gap();
    held.fighters_held = true;
    assert(gap.tick(held, GapMapping{}).has_value());
}

}  // namespace

int main() {
    ground_is_no_gap();
    air_with_floor_below_is_no_gap();
    air_with_a_platform_below_is_no_gap();
    a_gap_sounds_centred_and_level();
    the_hold_on_leaving_a_gap();
    knocked_out_is_silent_at_once();
    silent_while_paused_before_go_and_outside_a_match();
    std::cout << "gap_warning: all tests passed\n";
    return 0;
}
