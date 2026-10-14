/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The position cues' driver against hand-written match snapshots on
 * Final Destination's frame as measured (docs/a11y/gameplay-state.md,
 * "Sizes measured"): pan at the centre, the edges, past them and at the
 * blast zones; pitch at the floor, the camera box's top and the bottom
 * blast zone; the interval at, between and beyond the near and far
 * distances; no opponent; and when the cue is silent. */
#include "position_cue.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>

namespace {

using a11y::PositionMapping;
using a11y::VoiceTargets;

bool near(float a, float b, float tolerance = 1e-4f) {
    return std::fabs(a - b) <= tolerance;
}

A11yStageFrame final_destination() {
    A11yStageFrame stage{};
    stage.floor_left = -85.6f;
    stage.floor_right = 85.6f;
    stage.floor_y = 0.0f;
    stage.blast_left = -246.0f;
    stage.blast_right = 246.0f;
    stage.blast_top = 188.0f;
    stage.blast_bottom = -140.0f;
    stage.camera_left = -170.0f;
    stage.camera_right = 170.0f;
    stage.camera_top = 114.0f;
    stage.camera_bottom = -80.0f;
    return stage;
}

A11yMatchFighter fighter(int slot, float x, float y) {
    A11yMatchFighter f{};
    f.slot = slot;
    f.x = x;
    f.y = y;
    f.can_move = true;
    return f;
}

/* You at (x, y) on Final Destination, the opponent standing at the
 * centre. */
A11yMatchState match_at(float x, float y) {
    A11yMatchState match{};
    match.running = true;
    match.you = fighter(0, x, y);
    match.opponent_count = 1;
    match.opponents[0] = fighter(1, 0.0f, 0.0f);
    match.stage = final_destination();
    return match;
}

void pan_follows_x() {
    const A11yStageFrame stage = final_destination();
    assert(near(a11y::stage_pan(stage, 0.0f, 0.7f), 0.0f));
    assert(near(a11y::stage_pan(stage, 85.6f, 0.7f), 0.7f));
    assert(near(a11y::stage_pan(stage, -85.6f, 0.7f), -0.7f));
    /* Halfway to an edge, halfway to the edge pan. */
    assert(near(a11y::stage_pan(stage, 42.8f, 0.7f), 0.35f));
    /* Past an edge the pan keeps moving, halfway to the blast zone. */
    float halfway_out = (85.6f + 246.0f) / 2.0f;
    assert(near(a11y::stage_pan(stage, halfway_out, 0.7f), 0.85f));
    assert(near(a11y::stage_pan(stage, -halfway_out, 0.7f), -0.85f));
    assert(near(a11y::stage_pan(stage, 246.0f, 0.7f), 1.0f));
    assert(near(a11y::stage_pan(stage, -246.0f, 0.7f), -1.0f));
    assert(near(a11y::stage_pan(stage, 400.0f, 0.7f), 1.0f));
    assert(near(a11y::stage_pan(stage, -400.0f, 0.7f), -1.0f));
    /* The centre is the main floor's middle, not x = 0. */
    A11yStageFrame shifted = stage;
    shifted.floor_left = -60.0f;
    shifted.floor_right = 100.0f;
    assert(near(a11y::stage_pan(shifted, 20.0f, 0.7f), 0.0f));
    assert(near(a11y::stage_pan(shifted, 100.0f, 0.7f), 0.7f));
    assert(near(a11y::stage_pan(shifted, -60.0f, 0.7f), -0.7f));
    /* An edge pan of 1 puts the edges fully to the sides. */
    assert(near(a11y::stage_pan(stage, 85.6f, 1.0f), 1.0f));
    assert(near(a11y::stage_pan(stage, 150.0f, 1.0f), 1.0f));
}

void pitch_follows_y() {
    const A11yStageFrame stage = final_destination();
    assert(near(a11y::stage_pitch(stage, 0.0f, 1.0f, 1.0f), 1.0f));
    assert(near(a11y::stage_pitch(stage, 114.0f, 1.0f, 1.0f), 2.0f));
    assert(near(a11y::stage_pitch(stage, -140.0f, 1.0f, 1.0f), 0.5f));
    /* Linear in octaves: halfway up is half an octave. */
    assert(near(a11y::stage_pitch(stage, 57.0f, 1.0f, 1.0f), std::sqrt(2.0f)));
    assert(near(a11y::stage_pitch(stage, -70.0f, 1.0f, 1.0f), 1.0f / std::sqrt(2.0f)));
    /* Held beyond the camera's top and the bottom blast zone. */
    assert(near(a11y::stage_pitch(stage, 180.0f, 1.0f, 1.0f), 2.0f));
    assert(near(a11y::stage_pitch(stage, -200.0f, 1.0f, 1.0f), 0.5f));
    /* The ranges set the octaves. */
    assert(near(a11y::stage_pitch(stage, 114.0f, 2.0f, 0.5f), 4.0f));
    assert(near(a11y::stage_pitch(stage, -140.0f, 2.0f, 0.5f), 1.0f / std::sqrt(2.0f)));
    assert(near(a11y::stage_pitch(stage, 114.0f, 0.0f, 0.0f), 1.0f));
    /* The height reference is the main floor's, not y = 0. */
    A11yStageFrame raised = stage;
    raised.floor_y = 14.0f;
    assert(near(a11y::stage_pitch(raised, 14.0f, 1.0f, 1.0f), 1.0f));
    assert(near(a11y::stage_pitch(raised, 64.0f, 1.0f, 1.0f), std::sqrt(2.0f)));
}

void interval_follows_the_distance() {
    const PositionMapping mapping;
    /* At and beyond the far distance, and with no opponent: slow. */
    assert(near(a11y::nearness_interval(120.0f, mapping), 0.8f));
    assert(near(a11y::nearness_interval(500.0f, mapping), 0.8f));
    assert(near(a11y::nearness_interval(std::nullopt, mapping), 0.8f));
    /* At and within the near distance: fast. */
    assert(near(a11y::nearness_interval(10.0f, mapping), 0.08f));
    assert(near(a11y::nearness_interval(0.0f, mapping), 0.08f));
    /* Halfway: the geometric mean of the two intervals. */
    assert(near(a11y::nearness_interval(65.0f, mapping), std::sqrt(0.8f * 0.08f)));
    /* Equal steps of distance give equal ratios of interval. */
    float a = a11y::nearness_interval(100.0f, mapping);
    float b = a11y::nearness_interval(80.0f, mapping);
    float c = a11y::nearness_interval(60.0f, mapping);
    assert(near(a / b, b / c));
    assert(a > b && b > c);
    /* A near distance past the far one: fast within it, slow beyond. */
    PositionMapping crossed = mapping;
    crossed.near_distance = 150.0f;
    assert(near(a11y::nearness_interval(140.0f, crossed), 0.08f));
    assert(near(a11y::nearness_interval(160.0f, crossed), 0.8f));
}

void your_position_from_the_snapshot() {
    const PositionMapping mapping;
    std::optional<VoiceTargets> targets = a11y::your_position(match_at(85.6f, 114.0f), mapping);
    assert(targets.has_value());
    assert(near(targets->pan, 0.6f));
    assert(near(targets->pitch, 2.0f));
    assert(near(targets->gain, 1.0f));
    /* The opponent at the centre, about 142 units away: slow. */
    assert(near(targets->interval, 0.8f));

    /* Near the opponent: fast. */
    targets = a11y::your_position(match_at(5.0f, 0.0f), mapping);
    assert(targets.has_value() && near(targets->interval, 0.08f));
    /* Distance in a straight line: 30 to the side and 40 up is 50. */
    targets = a11y::your_position(match_at(30.0f, 40.0f), mapping);
    assert(targets.has_value() && near(targets->interval, a11y::nearness_interval(50.0f, mapping)));

    /* No opponent: the slow pulse. */
    A11yMatchState alone = match_at(5.0f, 0.0f);
    alone.opponent_count = 0;
    targets = a11y::your_position(alone, mapping);
    assert(targets.has_value() && near(targets->interval, 0.8f));
}

void silent_when_it_should_be() {
    const PositionMapping mapping;
    A11yMatchState match = match_at(0.0f, 0.0f);
    assert(a11y::your_position(match, mapping).has_value());

    A11yMatchState knocked_out = match;
    knocked_out.you.out = true;
    assert(!a11y::your_position(knocked_out, mapping).has_value());

    A11yMatchState paused = match;
    paused.paused = true;
    assert(!a11y::your_position(paused, mapping).has_value());

    /* Before GO. */
    A11yMatchState ready = match;
    ready.you.can_move = false;
    assert(!a11y::your_position(ready, mapping).has_value());

    /* A slow-motion mode's skipped tick keeps the cue going. */
    A11yMatchState slow_motion = match;
    slow_motion.fighters_held = true;
    assert(a11y::your_position(slow_motion, mapping).has_value());

    assert(!a11y::your_position(A11yMatchState{}, mapping).has_value());
}

}  // namespace

int main() {
    pan_follows_x();
    pitch_follows_y();
    interval_follows_the_distance();
    your_position_from_the_snapshot();
    silent_when_it_should_be();
    std::cout << "position_cue: all tests passed\n";
    return 0;
}
