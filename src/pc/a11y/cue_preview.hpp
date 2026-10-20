/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What the cues' previews share (primer docs/a11y/cues.md, "Preview"):
 * about two seconds of a cue at a fixed, typical moment of a match, played
 * outside any match. Each cue's moment sits beside its driver as a pure
 * function of the seconds since the preview began, which returns none once
 * the preview has ended (your_position_preview and the others); MatchCues
 * plays them on voices of their own. */
#pragma once
#include <algorithm>
#include <cmath>

namespace a11y {

/* About how long a preview lasts. */
constexpr double kPreviewSeconds = 2.0;

/* How long a pulsed voice plays to give this many pulses, the first at its
 * start: until 0.6 of an interval after the last pulse begins, after the
 * pulse's sound, which ends inside the voice's fastest interval, and before
 * the next would begin. */
inline double pulses_seconds(float interval, int pulses) {
    return (pulses - 1 + 0.6) * static_cast<double>(interval);
}

/* How many pulses a pulsed voice's preview plays at an interval: the count
 * whose pulses_seconds comes nearest kPreviewSeconds, and at least two, so
 * the pace is heard. */
inline int preview_pulses(float interval) {
    if (!(interval > 0.0f)) {
        return 2;
    }
    const double nearest = kPreviewSeconds / static_cast<double>(interval) + 0.4;
    return std::max(2, static_cast<int>(std::lround(nearest)));
}

}  // namespace a11y
