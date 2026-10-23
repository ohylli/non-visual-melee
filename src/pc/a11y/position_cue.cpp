/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "position_cue.hpp"
#include "cue_preview.hpp"
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

/* A position cue's preview: centred, at the cue's own pitch, pulsing at
 * the interval. */
std::optional<VoiceTargets> centred_pulses(double seconds, float interval) {
    if (seconds >= pulses_seconds(interval, preview_pulses(interval))) {
        return std::nullopt;
    }
    VoiceTargets targets;
    targets.interval = interval;
    return targets;
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

int opponent_side(float dx, int last_side, float margin) {
    if (last_side < 0) {
        return dx > margin ? 1 : -1;
    }
    if (last_side > 0) {
        return dx < -margin ? -1 : 1;
    }
    return dx < 0.0f ? -1 : 1;
}

float relative_pan(float dx, int side, const OpponentMapping& mapping) {
    /* 0 level with you, 1 at the side distance. Within the crossing margin
     * past you the distance still counts, on the side kept. */
    const float t = mapping.side_distance > 0.0f ?
                        std::clamp(std::fabs(dx) / mapping.side_distance, 0.0f, 1.0f) :
                        1.0f;
    return static_cast<float>(side) * (mapping.pan_floor + (1.0f - mapping.pan_floor) * t);
}

float relative_pitch(float dy, const OpponentMapping& mapping) {
    float t; /* -1 at the height distance below, 1 above */
    if (mapping.height_distance > 0.0f) {
        t = std::clamp(dy / mapping.height_distance, -1.0f, 1.0f);
    } else {
        t = dy > 0.0f ? 1.0f : (dy < 0.0f ? -1.0f : 0.0f);
    }
    return std::exp2(mapping.stage.pitch_range * 0.5f * t);
}

std::optional<VoiceTargets> OpponentPosition::tick(
    const A11yMatchState& match, const OpponentMapping& mapping) {
    /* Both position cues start at GO, when your input comes on. */
    const bool silent = !match.running || match.paused || !match.you.can_move ||
                        match.opponent_count == 0 || match.opponents[0].out ||
                        (mapping.relative && match.you.out);
    if (silent) {
        forget();
        return std::nullopt;
    }
    const A11yMatchFighter& opponent = match.opponents[0];
    if (!mapping.relative) {
        forget();
        VoiceTargets targets = stage_targets(match.stage, opponent, mapping.stage);
        targets.interval = mapping.interval;
        return targets;
    }
    if (opponent.slot != m_slot) {
        forget();
        m_slot = opponent.slot;
    }
    const float dx = opponent.x - match.you.x;
    m_side = opponent_side(dx, m_side, mapping.crossing_margin);
    VoiceTargets targets;
    targets.pan = relative_pan(dx, m_side, mapping);
    targets.pitch = relative_pitch(opponent.y - match.you.y, mapping);
    targets.gain = 1.0f;
    targets.interval = mapping.interval;
    return targets;
}

std::optional<VoiceTargets> your_position_preview(double seconds, const YourMapping& mapping) {
    return centred_pulses(seconds, mapping.slow_interval);
}

std::optional<VoiceTargets> opponent_position_preview(
    double seconds, const OpponentMapping& mapping) {
    std::optional<VoiceTargets> targets = centred_pulses(seconds, mapping.interval);
    if (targets.has_value() && mapping.relative) {
        /* The side changes midway between pulses, so neither moves while
         * it sounds. */
        const int left_pulses = preview_pulses(mapping.interval) / 2;
        const double change = (left_pulses - 0.5) * static_cast<double>(mapping.interval);
        targets->pan = seconds < change ? -mapping.pan_floor : mapping.pan_floor;
    }
    return targets;
}

}  // namespace a11y
