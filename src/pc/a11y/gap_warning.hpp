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

private:
    int m_hold = 0; /* ticks left to sound with no gap */
};

}  // namespace a11y
