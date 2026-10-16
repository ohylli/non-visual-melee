/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The position cues' driver against hand-written match snapshots on
 * Final Destination's frame as measured (docs/a11y/gameplay-state.md,
 * "Sizes measured"): pan at the centre, between it and the blast zones, at
 * them and beyond, and framed by the camera box instead; pitch midway, at the camera box's top and
 * the bottom blast zone; the interval at, between and beyond the near and far distances; no
 * opponent; and when the cue is silent. The opponent's cue: the same mapping on the opponent's
 * position with its own values, its fixed interval, no opponent, the opponent knocked out and a
 * change of opponent. */
#include "position_cue.hpp"
#include "test_match_snapshot.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>

namespace {

using namespace match_test;
using a11y::OpponentMapping;
using a11y::VoiceTargets;
using a11y::YourMapping;

bool near(float a, float b, float tolerance = 1e-4f) {
    return std::fabs(a - b) <= tolerance;
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
    assert(near(a11y::stage_pan(stage, 0.0f, false), 0.0f));
    assert(near(a11y::stage_pan(stage, 246.0f, false), 1.0f));
    assert(near(a11y::stage_pan(stage, -246.0f, false), -1.0f));
    /* Linear: halfway to a blast zone is halfway to the side, and the main
     * floor's ends are no mark. */
    assert(near(a11y::stage_pan(stage, 123.0f, false), 0.5f));
    assert(near(a11y::stage_pan(stage, -123.0f, false), -0.5f));
    assert(near(a11y::stage_pan(stage, 85.6f, false), 85.6f / 246.0f));
    /* Held beyond the blast zones. */
    assert(near(a11y::stage_pan(stage, 400.0f, false), 1.0f));
    assert(near(a11y::stage_pan(stage, -400.0f, false), -1.0f));
    /* The centre is the blast zones' middle, not x = 0. */
    A11yStageFrame shifted = stage;
    shifted.blast_left = -200.0f;
    shifted.blast_right = 240.0f;
    assert(near(a11y::stage_pan(shifted, 20.0f, false), 0.0f));
    assert(near(a11y::stage_pan(shifted, 130.0f, false), 0.5f));
    assert(near(a11y::stage_pan(shifted, -200.0f, false), -1.0f));
}

void pan_to_the_camera_follows_x() {
    const A11yStageFrame stage = final_destination();
    assert(near(a11y::stage_pan(stage, 0.0f, true), 0.0f));
    assert(near(a11y::stage_pan(stage, 170.0f, true), 1.0f));
    assert(near(a11y::stage_pan(stage, -170.0f, true), -1.0f));
    assert(near(a11y::stage_pan(stage, 85.0f, true), 0.5f));
    /* Held from the camera box's sides out to the blast zones and
     * beyond. */
    assert(near(a11y::stage_pan(stage, 210.0f, true), 1.0f));
    assert(near(a11y::stage_pan(stage, -246.0f, true), -1.0f));
    /* The centre is the camera box's middle. */
    A11yStageFrame shifted = stage;
    shifted.camera_left = -150.0f;
    shifted.camera_right = 190.0f;
    assert(near(a11y::stage_pan(shifted, 20.0f, true), 0.0f));
    assert(near(a11y::stage_pan(shifted, 105.0f, true), 0.5f));
}

void pitch_follows_y() {
    const A11yStageFrame stage = final_destination();
    /* Midway between the bottom blast zone and the camera box's top. */
    const float middle = (-140.0f + 114.0f) / 2.0f;
    assert(near(a11y::stage_pitch(stage, middle, 2.0f), 1.0f));
    assert(near(a11y::stage_pitch(stage, 114.0f, 2.0f), 2.0f));
    assert(near(a11y::stage_pitch(stage, -140.0f, 2.0f), 0.5f));
    /* Linear in octaves: halfway up from the middle is half an octave. */
    assert(near(a11y::stage_pitch(stage, (middle + 114.0f) / 2.0f, 2.0f), std::sqrt(2.0f)));
    assert(near(a11y::stage_pitch(stage, (middle - 140.0f) / 2.0f, 2.0f), 1.0f / std::sqrt(2.0f)));
    /* The main floor is no mark: on Final Destination it sits a little
     * above the middle. */
    assert(near(a11y::stage_pitch(stage, 0.0f, 2.0f), std::exp2(2.0f * (140.0f / 254.0f - 0.5f))));
    /* Held beyond the camera's top and the bottom blast zone. */
    assert(near(a11y::stage_pitch(stage, 180.0f, 2.0f), 2.0f));
    assert(near(a11y::stage_pitch(stage, -200.0f, 2.0f), 0.5f));
    /* The range sets the octaves, half down and half up. */
    assert(near(a11y::stage_pitch(stage, 114.0f, 1.0f), std::sqrt(2.0f)));
    assert(near(a11y::stage_pitch(stage, -140.0f, 1.0f), 1.0f / std::sqrt(2.0f)));
    assert(near(a11y::stage_pitch(stage, 114.0f, 0.0f), 1.0f));
    /* The bottom is the blast zone's, below the camera box's. */
    assert(a11y::stage_pitch(stage, -110.0f, 2.0f) > a11y::stage_pitch(stage, -130.0f, 2.0f));
}

void interval_follows_the_distance() {
    const YourMapping mapping;
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
    YourMapping crossed = mapping;
    crossed.near_distance = 150.0f;
    assert(near(a11y::nearness_interval(140.0f, crossed), 0.08f));
    assert(near(a11y::nearness_interval(160.0f, crossed), 0.8f));
}

void your_position_from_the_snapshot() {
    const YourMapping mapping;
    std::optional<VoiceTargets> targets = a11y::your_position(match_at(85.0f, 114.0f), mapping);
    assert(targets.has_value());
    assert(near(targets->pan, 0.5f));
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
    const YourMapping mapping;
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

/* You at the centre, the opponent at (x, y). */
A11yMatchState opponent_at(float x, float y) {
    A11yMatchState match = match_at(0.0f, 0.0f);
    match.opponents[0] = fighter(1, x, y);
    return match;
}

void the_opponent_cue_maps_the_opponents_position() {
    const OpponentMapping mapping;
    std::optional<VoiceTargets> targets =
        a11y::opponent_position(opponent_at(85.0f, 114.0f), mapping);
    assert(targets.has_value());
    assert(near(targets->pan, 0.5f));
    assert(near(targets->pitch, 2.0f));
    assert(near(targets->gain, 1.0f));
    targets = a11y::opponent_position(opponent_at(-246.0f, -140.0f), mapping);
    assert(targets.has_value() && near(targets->pan, -1.0f) && near(targets->pitch, 0.5f));
    /* The same mapping as yours, where you stand makes no difference. */
    A11yMatchState moved = opponent_at(42.8f, 57.0f);
    moved.you = fighter(0, -70.0f, 30.0f);
    targets = a11y::opponent_position(moved, mapping);
    std::optional<VoiceTargets> yours = a11y::your_position(match_at(42.8f, 57.0f), YourMapping{});
    assert(targets.has_value() && yours.has_value());
    assert(near(targets->pan, yours->pan) && near(targets->pitch, yours->pitch));
    /* Its own copy of the values. */
    OpponentMapping narrow = mapping;
    narrow.stage.pitch_range = 1.0f;
    targets = a11y::opponent_position(opponent_at(123.0f, 114.0f), narrow);
    assert(targets.has_value() && near(targets->pitch, std::sqrt(2.0f)));
    /* The pan frame is the mapping's: the camera box's by default, else
     * the blast zones'. */
    OpponentMapping blast = mapping;
    blast.stage.pan_to_camera = false;
    targets = a11y::opponent_position(opponent_at(85.0f, 0.0f), blast);
    assert(targets.has_value() && near(targets->pan, 85.0f / 246.0f));
    YourMapping your_blast;
    your_blast.stage.pan_to_camera = false;
    yours = a11y::your_position(match_at(85.0f, 0.0f), your_blast);
    assert(yours.has_value() && near(yours->pan, 85.0f / 246.0f));
}

void the_opponent_cue_keeps_its_interval() {
    OpponentMapping mapping;
    for (float x : {2.0f, 30.0f, 85.6f, 200.0f}) {
        std::optional<VoiceTargets> targets =
            a11y::opponent_position(opponent_at(x, 0.0f), mapping);
        assert(targets.has_value() && near(targets->interval, 0.4f));
    }
    mapping.interval = 0.65f;
    std::optional<VoiceTargets> targets = a11y::opponent_position(opponent_at(5.0f, 0.0f), mapping);
    assert(targets.has_value() && near(targets->interval, 0.65f));
}

void the_opponent_cue_is_silent_when_it_should_be() {
    const OpponentMapping mapping;
    A11yMatchState match = opponent_at(30.0f, 0.0f);
    assert(a11y::opponent_position(match, mapping).has_value());

    A11yMatchState alone = match;
    alone.opponent_count = 0;
    assert(!a11y::opponent_position(alone, mapping).has_value());

    A11yMatchState knocked_out = match;
    knocked_out.opponents[0].out = true;
    assert(!a11y::opponent_position(knocked_out, mapping).has_value());

    A11yMatchState paused = match;
    paused.paused = true;
    assert(!a11y::opponent_position(paused, mapping).has_value());

    /* Before GO. */
    A11yMatchState ready = match;
    ready.you.can_move = false;
    assert(!a11y::opponent_position(ready, mapping).has_value());

    /* While you are knocked out, the opponent can still be heard. */
    A11yMatchState you_out = match;
    you_out.you.out = true;
    assert(a11y::opponent_position(you_out, mapping).has_value());

    A11yMatchState slow_motion = match;
    slow_motion.fighters_held = true;
    assert(a11y::opponent_position(slow_motion, mapping).has_value());

    assert(!a11y::opponent_position(A11yMatchState{}, mapping).has_value());
}

void the_opponent_cue_follows_a_change_of_opponent() {
    const OpponentMapping mapping;
    A11yMatchState match = opponent_at(-40.0f, 0.0f);
    std::optional<VoiceTargets> before = a11y::opponent_position(match, mapping);
    /* Another fighter comes nearer: the snapshot's choice moves the cue. */
    match.opponents[0] = fighter(2, 60.0f, 50.0f);
    std::optional<VoiceTargets> after = a11y::opponent_position(match, mapping);
    assert(before.has_value() && after.has_value());
    assert(before->pan < 0.0f && after->pan > 0.0f && after->pitch > before->pitch);
    assert(near(after->interval, before->interval));
}

}  // namespace

int main() {
    pan_follows_x();
    pan_to_the_camera_follows_x();
    pitch_follows_y();
    interval_follows_the_distance();
    your_position_from_the_snapshot();
    silent_when_it_should_be();
    the_opponent_cue_maps_the_opponents_position();
    the_opponent_cue_keeps_its_interval();
    the_opponent_cue_is_silent_when_it_should_be();
    the_opponent_cue_follows_a_change_of_opponent();
    std::cout << "position_cue: all tests passed\n";
    return 0;
}
