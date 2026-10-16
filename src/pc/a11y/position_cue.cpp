/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "position_cue.hpp"
#include "match_state.hpp"
#include <algorithm>
#include <cmath>

namespace a11y {
namespace {

/* How far v has gone from `from` towards `to`, 0 to 1, either way round.
 * Where the two meet, a frame with no width: midway, so a fighter sounds at
 * the centre and the base pitch. */
float progress(float v, float from, float to) {
    if (from == to) {
        return 0.5f;
    }
    return std::clamp((v - from) / (to - from), 0.0f, 1.0f);
}

}  // namespace

float stage_pan(const A11yStageFrame& stage, float x, bool to_camera) {
    const float left = to_camera ? stage.camera_left : stage.blast_left;
    const float right = to_camera ? stage.camera_right : stage.blast_right;
    return 2.0f * progress(x, left, right) - 1.0f;
}

float stage_pitch(const A11yStageFrame& stage, float y, float range) {
    return std::exp2(range * (progress(y, stage.blast_bottom, stage.camera_top) - 0.5f));
}

float nearness_interval(std::optional<float> distance, const YourMapping& mapping) {
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

VoiceTargets stage_targets(
    const A11yStageFrame& stage, const A11yMatchFighter& fighter, const StageMapping& mapping) {
    VoiceTargets targets;
    targets.pan = stage_pan(stage, fighter.x, mapping.pan_to_camera);
    targets.pitch = stage_pitch(stage, fighter.y, mapping.pitch_range);
    targets.gain = 1.0f;
    return targets;
}

std::optional<VoiceTargets> your_position(const A11yMatchState& match, const YourMapping& mapping) {
    if (!you_in_play(match)) {
        return std::nullopt;
    }
    const A11yMatchFighter& you = match.you;
    std::optional<float> distance;
    if (match.opponent_count > 0) {
        const A11yMatchFighter& opponent = match.opponents[0];
        distance = std::hypot(opponent.x - you.x, opponent.y - you.y);
    }
    VoiceTargets targets = stage_targets(match.stage, you, mapping.stage);
    targets.interval = nearness_interval(distance, mapping);
    return targets;
}

std::optional<VoiceTargets> opponent_position(
    const A11yMatchState& match, const OpponentMapping& mapping) {
    /* Both position cues start at GO, when your input comes on. */
    if (!match.running || match.paused || !match.you.can_move || match.opponent_count == 0) {
        return std::nullopt;
    }
    const A11yMatchFighter& opponent = match.opponents[0];
    if (opponent.out) {
        return std::nullopt;
    }
    VoiceTargets targets = stage_targets(match.stage, opponent, mapping.stage);
    targets.interval = mapping.interval;
    return targets;
}

}  // namespace a11y
