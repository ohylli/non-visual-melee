/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The gap warning's driver (primer docs/a11y/cues.md, "The gap warning"): a
 * held, centred tone while you are in the air with no floor below you, so
 * that falling straight down would take you past the bottom blast zone. The
 * snapshot's downward cast decides; a short hold on leaving a gap keeps the
 * tone from flickering at a floor's edge. */
#pragma once
#include "cue_engine.hpp"
#include "game_access.h"
#include <optional>

namespace a11y {

/* The gap warning's mapping, as its settings hold it. */
struct GapMapping {
    /* Ticks the warning sounds on after you leave a gap. */
    int hold_ticks = 10;
};

/* How long the gap warning's preview takes to rise to full and to fall
 * away again. */
constexpr double kGapPreviewRamp = 0.3;

/* The snapshot's gap: you are in the air and it found no floor below you.
 * Whether you are in play is the driver's (you_in_play). */
bool over_gap(const A11yMatchState& match);

/* The driver: over_gap with the hold, whose count is its only state. */
class GapWarning {
public:
    /* Once a tick: the voice's targets, or none while it is silent. Silent
     * outside a match, while paused, before GO and while you are out, at
     * once, hold or not. */
    std::optional<VoiceTargets> tick(const A11yMatchState& match, const GapMapping& mapping);
    /* The hold forgotten: the next tick decides from its snapshot alone. */
    void forget() { m_hold = 0; }

private:
    int m_hold = 0; /* ticks left to sound with no gap */
};

/* The gap warning's preview (cue_preview.hpp), for the seconds since the
 * preview began, none once it has ended: the hum, centred at its own
 * pitch, rising to full over kGapPreviewRamp, held, and falling away again
 * by kPreviewSeconds. */
std::optional<VoiceTargets> gap_warning_preview(double seconds);

}  // namespace a11y
