/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gap_warning.hpp"
#include "match_state.hpp"

namespace a11y {

bool over_gap(const A11yMatchState& match) {
    return match.you.airborne && !match.below.found;
}

std::optional<VoiceTargets> GapWarning::tick(
    const A11yMatchState& match, const GapMapping& mapping) {
    if (!you_in_play(match)) {
        m_hold = 0;
        return std::nullopt;
    }
    if (over_gap(match)) {
        m_hold = mapping.hold_ticks;
    } else if (m_hold > 0) {
        m_hold--;
    } else {
        return std::nullopt;
    }
    /* Centred, at its own pitch and full gain: the tone says only that
     * there is a gap. */
    return VoiceTargets{};
}

}  // namespace a11y
