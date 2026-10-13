/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "position_cue.hpp"
#include <algorithm>
#include <cmath>

namespace a11y {
namespace {

/* How far v has gone from `from` towards `to`, 0 to 1, either way round.
 * Where the two meet, the callers ask only from beyond them: all the way. */
float progress(float v, float from, float to) {
    if (from == to) {
        return 1.0f;
    }
    return std::clamp((v - from) / (to - from), 0.0f, 1.0f);
}

}  // namespace

float stage_pan(const A11yStageFrame& stage, float x, float edge_pan) {
    const float centre = (stage.floor_left + stage.floor_right) / 2.0f;
    if (x >= centre) {
        if (x <= stage.floor_right) {
            return edge_pan * progress(x, centre, stage.floor_right);
        }
        return edge_pan + (1.0f - edge_pan) * progress(x, stage.floor_right, stage.blast_right);
    }
    if (x >= stage.floor_left) {
        return -edge_pan * progress(x, centre, stage.floor_left);
    }
    return -edge_pan - (1.0f - edge_pan) * progress(x, stage.floor_left, stage.blast_left);
}

float stage_pitch(const A11yStageFrame& stage, float y, float up_range, float down_range) {
    const float floor = stage.floor_y;
    const float octaves = y >= floor ? up_range * progress(y, floor, stage.camera_top) :
                                       -down_range * progress(y, floor, stage.blast_bottom);
    return std::exp2(octaves);
}

float nearness_interval(std::optional<float> distance, const PositionMapping& mapping) {
    if (!distance.has_value()) {
        return mapping.slow_interval;
    }
    float t;
    if (mapping.near_distance >= mapping.far_distance) {
        t = *distance <= mapping.near_distance ? 1.0f : 0.0f;
    } else {
        t = progress(*distance, mapping.far_distance, mapping.near_distance);
    }
    const float slow = std::log(mapping.slow_interval);
    const float fast = std::log(mapping.fast_interval);
    return std::exp(slow + (fast - slow) * t);
}

std::optional<VoiceTargets> your_position(
    const A11yMatchState& match, const PositionMapping& mapping) {
    const A11yMatchFighter& you = match.you;
    if (!match.running || match.paused || !you.can_move || you.out) {
        return std::nullopt;
    }
    std::optional<float> distance;
    if (match.opponent_count > 0) {
        const A11yMatchFighter& opponent = match.opponents[0];
        distance = std::hypot(opponent.x - you.x, opponent.y - you.y);
    }
    VoiceTargets targets;
    targets.pan = stage_pan(match.stage, you.x, mapping.edge_pan);
    targets.pitch = stage_pitch(match.stage, you.y, mapping.up_range, mapping.down_range);
    targets.gain = 1.0f;
    targets.interval = nearness_interval(distance, mapping);
    return targets;
}

}  // namespace a11y
