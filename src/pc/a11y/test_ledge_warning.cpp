/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The ledge warning's driver against hand-written match snapshots on Final
 * Destination's and Battlefield's floors as measured
 * (docs/a11y/gameplay-state.md, "Sizes measured"): a deadly end on each
 * side, both ends of a small floor, a platform's safe ends over the main
 * floor, on the ground and in the air, the pan and loudness at the warning
 * distance and at the edge, and silent with no floor below, while knocked
 * out, paused, before GO and outside a match. */
#include "ledge_warning.hpp"
#include "test_match_snapshot.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {

using namespace match_test;
using a11y::LedgeMapping;
using a11y::LedgeTargets;

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

/* Line ids as on Final Destination and Battlefield: the main floor's
 * middle piece, and a platform. */
constexpr int kMainFloor = 1;
constexpr int kPlatform = 20;

/* You at (x, y) on Final Destination, the opponent at the centre, with
 * Final Destination's main floor below you, both its ends deadly. */
A11yMatchState match_at(float x, float y, bool airborne) {
    A11yMatchState match{};
    match.running = true;
    match.you = fighter(0, x, y);
    match.you.airborne = airborne;
    match.below.found = true;
    match.below.line = kMainFloor;
    match.below.y = 0.0f;
    match.below.left = A11yFloorEnd{-85.6f, 0.0f, true};
    match.below.right = A11yFloorEnd{85.6f, 0.0f, true};
    match.opponent_count = 1;
    match.opponents[0] = fighter(1, 0.0f, 0.0f);
    match.stage = final_destination();
    return match;
}

A11yMatchState standing(float x) {
    return match_at(x, 0.0f, false);
}

LedgeTargets warn(const A11yMatchState& match) {
    return a11y::ledge_warning(match, LedgeMapping{});
}

void silent_in_the_middle() {
    LedgeTargets t = warn(standing(0.0f));
    assert(!t.left.has_value() && !t.right.has_value());
    /* Just beyond the warning distance from either end. */
    t = warn(standing(-85.6f + 40.1f));
    assert(!t.left.has_value() && !t.right.has_value());
    t = warn(standing(85.6f - 40.1f));
    assert(!t.left.has_value() && !t.right.has_value());
}

void a_deadly_end_on_each_side() {
    LedgeTargets left = warn(standing(-70.0f));
    assert(left.left.has_value() && !left.right.has_value());
    assert(left.left->pan < 0.0f);
    assert(left.left->pitch == 1.0f && left.left->interval == a11y::kLedgeInterval);
    LedgeTargets right = warn(standing(70.0f));
    assert(right.right.has_value() && !right.left.has_value());
    assert(right.right->pan > 0.0f);
    /* The same distance, mirrored. */
    assert(near(right.right->pan, -left.left->pan));
    assert(near(right.right->gain, left.left->gain));
}

void pan_and_loudness_from_the_warning_distance_to_the_edge() {
    /* At the warning distance: fully to the side, at the level floor. */
    LedgeTargets far = warn(standing(-85.6f + 40.0f));
    assert(far.left.has_value());
    assert(near(far.left->pan, -1.0f) && near(far.left->gain, 0.5f));
    /* Halfway: halfway in both. */
    LedgeTargets half = warn(standing(85.6f - 20.0f));
    assert(half.right.has_value());
    assert(near(half.right->pan, 0.575f) && near(half.right->gain, 0.75f));
    /* At the edge, teetering: the pan floor, full. */
    LedgeTargets edge = warn(standing(-85.6f));
    assert(near(edge.left->pan, -0.15f) && near(edge.left->gain, 1.0f));
    /* Nearer is louder and more central all the way. */
    float last_gain = 0.0f;
    float last_pan = 1.1f;
    for (float d = 40.0f; d >= 0.0f; d -= 1.0f) {
        auto t = warn(standing(85.6f - d)).right;
        assert(t.has_value());
        assert(t->gain > last_gain && t->pan < last_pan);
        last_gain = t->gain;
        last_pan = t->pan;
    }
    /* The mapping's values move both ends. */
    LedgeMapping wide{60.0f, 0.5f, 0.1f};
    auto wider = a11y::ledge_warning(standing(85.6f - 60.0f), wide).right;
    assert(wider.has_value() && near(wider->pan, 1.0f) && near(wider->gain, 0.1f));
    auto at = a11y::ledge_warning(standing(85.6f), wide).right;
    assert(near(at->pan, 0.5f) && near(at->gain, 1.0f));
}

void in_the_air_as_on_the_ground() {
    /* Jumping above the main floor near its left end, high or low. */
    LedgeTargets high = warn(match_at(-70.0f, 60.0f, true));
    LedgeTargets low = warn(standing(-70.0f));
    assert(high.left.has_value() && !high.right.has_value());
    assert(near(high.left->pan, low.left->pan) && near(high.left->gain, low.left->gain));
    /* A unit past the end, where the cast still meets the floor: as at the
     * edge. */
    LedgeTargets past = warn(match_at(-86.4f, -2.0f, true));
    assert(near(past.left->pan, -0.15f) && near(past.left->gain, 1.0f));
}

void both_ends_of_a_small_floor() {
    /* A floor 40 units wide with nothing below it, both ends deadly. */
    A11yMatchState cloud = standing(5.0f);
    cloud.below.line = kPlatform;
    cloud.below.left = A11yFloorEnd{-20.0f, 0.0f, true};
    cloud.below.right = A11yFloorEnd{20.0f, 0.0f, true};
    LedgeTargets both = warn(cloud);
    assert(both.left.has_value() && both.right.has_value());
    assert(both.left->pan < 0.0f && both.right->pan > 0.0f);
    /* The nearer end, the right, louder and more central. */
    assert(both.right->gain > both.left->gain);
    assert(both.right->pan < -both.left->pan);
}

void a_platforms_safe_ends_are_silent() {
    /* Standing at the left end of Battlefield's left platform, the main
     * floor below past both its ends. */
    A11yMatchState platform = match_at(-57.6f, 27.2f, false);
    platform.below.line = kPlatform;
    platform.below.y = 27.2f;
    platform.below.left = A11yFloorEnd{-57.6f, 27.2f, false};
    platform.below.right = A11yFloorEnd{-20.0f, 27.2f, false};
    LedgeTargets t = warn(platform);
    assert(!t.left.has_value() && !t.right.has_value());
    /* In the air above it, the same. */
    platform.you.airborne = true;
    platform.you.y = 50.0f;
    t = warn(platform);
    assert(!t.left.has_value() && !t.right.has_value());
    /* One end safe, the other deadly: only the deadly one sounds. */
    platform.below.right.deadly = true;
    platform.you.x = -30.0f;
    t = warn(platform);
    assert(!t.left.has_value() && t.right.has_value());
}

void silent_with_no_floor_below() {
    /* Past the edge, falling: the gap warning's. */
    A11yMatchState gap = match_at(-90.0f, -10.0f, true);
    gap.below = A11yFloorBelow{};
    gap.below.line = -1;
    LedgeTargets t = warn(gap);
    assert(!t.left.has_value() && !t.right.has_value());
    /* On the ground with an unreadable floor: nothing to say either. */
    A11yMatchState unreadable = standing(-80.0f);
    unreadable.below = A11yFloorBelow{};
    unreadable.below.line = -1;
    t = warn(unreadable);
    assert(!t.left.has_value() && !t.right.has_value());
}

void silent_while_out_paused_before_go_and_outside_a_match() {
    A11yMatchState out = standing(-80.0f);
    out.you.out = true;
    LedgeTargets t = warn(out);
    assert(!t.left.has_value() && !t.right.has_value());
    A11yMatchState paused = standing(-80.0f);
    paused.paused = true;
    t = warn(paused);
    assert(!t.left.has_value() && !t.right.has_value());
    A11yMatchState before_go = standing(-80.0f);
    before_go.you.can_move = false;
    t = warn(before_go);
    assert(!t.left.has_value() && !t.right.has_value());
    A11yMatchState none{};
    none.below.line = -1;
    t = warn(none);
    assert(!t.left.has_value() && !t.right.has_value());
    /* Slow motion's skipped ticks are no pause. */
    A11yMatchState held = standing(-80.0f);
    held.fighters_held = true;
    assert(warn(held).left.has_value());
    /* On the respawn platform, back in play over the middle: nothing near
     * an edge. */
    A11yMatchState respawned = match_at(16.0f, 112.9f, true);
    t = warn(respawned);
    assert(!t.left.has_value() && !t.right.has_value());
}

}  // namespace

int main() {
    silent_in_the_middle();
    a_deadly_end_on_each_side();
    pan_and_loudness_from_the_warning_distance_to_the_edge();
    in_the_air_as_on_the_ground();
    both_ends_of_a_small_floor();
    a_platforms_safe_ends_are_silent();
    silent_with_no_floor_below();
    silent_while_out_paused_before_go_and_outside_a_match();
    std::cout << "ledge_warning: all tests passed\n";
    return 0;
}
