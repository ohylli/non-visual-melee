/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ledge_warning.hpp"
#include "cue_preview.hpp"
#include "match_state.hpp"
#include <algorithm>

namespace a11y {
namespace {

/* A voice for a deadly end the given distance away on one side (-1 left,
 * 1 right), or none beyond the warning distance. Past the end counts as at
 * it. */
std::optional<VoiceTargets> ledge_side(float distance, int side, const LedgeMapping& mapping) {
    if (distance > mapping.distance) {
        return std::nullopt;
    }
    /* 0 at the edge, 1 at the warning distance. */
    const float t =
        mapping.distance > 0.0f ? std::clamp(distance / mapping.distance, 0.0f, 1.0f) : 0.0f;
    VoiceTargets targets;
    targets.pan = static_cast<float>(side) * (mapping.pan_floor + (1.0f - mapping.pan_floor) * t);
    targets.gain = 1.0f - (1.0f - mapping.level_floor) * t;
    targets.interval = kLedgeInterval;
    return targets;
}

}  // namespace

LedgeTargets ledge_warning(const A11yMatchState& match, const LedgeMapping& mapping) {
    const A11yFloorBelow& below = match.below;
    if (!you_in_play(match) || !below.found) {
        return LedgeTargets{};
    }
    LedgeTargets targets;
    const float x = match.you.x;
    if (below.left.deadly) {
        targets.left = ledge_side(x - below.left.x, -1, mapping);
    }
    if (below.right.deadly) {
        targets.right = ledge_side(below.right.x - x, 1, mapping);
    }
    return targets;
}

std::optional<LedgeTargets> ledge_warning_preview(double seconds, const LedgeMapping& mapping) {
    const double side = pulses_seconds(kLedgeInterval, kLedgePreviewPulses);
    const double right_from = (kLedgePreviewPulses + 1) * static_cast<double>(kLedgeInterval);
    if (seconds >= right_from + side) {
        return std::nullopt;
    }
    const float distance = mapping.distance / 2.0f;
    LedgeTargets targets;
    if (seconds < side) {
        targets.left = ledge_side(distance, -1, mapping);
    } else if (seconds >= right_from) {
        targets.right = ledge_side(distance, 1, mapping);
    }
    return targets;
}

}  // namespace a11y
