/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The ledge warning's driver (primer docs/a11y/cues.md, "The ledge
 * warning"): a pulsed tone on the left or right while you are near a deadly
 * end of the floor below you, on the ground and in the air. Pan and
 * loudness say how near, the Star Fox 64 fork's beside chord: fully to that
 * side at the warning distance, moving towards the centre as you close in
 * and stopping at the pan floor at the edge, while the level rises from the
 * level floor to full. With no floor below, the gap warning has taken
 * over. */
#pragma once
#include "cue_engine.hpp"
#include "game_access.h"
#include <optional>

namespace a11y {

/* The ledge warning's pulse rate, fixed: about 5 a second, an onset for
 * the ear to place, which a held tone panned by level alone does not
 * give. */
constexpr float kLedgeInterval = 0.2f;

/* How far past an end of the floor below you the snapshot's deadly test
 * casts by default (A11yFloorEnd, a11y_game_match_state's edge_offset):
 * beyond the unit a floor reaches where another line joins it, and inside
 * the 10.8 units from a Battlefield side platform's end to the main
 * floor's. */
constexpr float kEdgeOffset = 5.0f;

/* The ledge warning's mapping, as its settings hold it. */
struct LedgeMapping {
    /* World units from a deadly end at which its voice starts. */
    float distance = 40.0f;
    /* Pan at the edge itself, so the side stays clear. */
    float pan_floor = 0.3f;
    /* Gain at the warning distance, so the start is heard rather than a
     * fade from silence: 0 to 1. */
    float level_floor = 0.5f;
};

/* The ledge warning's two voices this tick, each none while silent. */
struct LedgeTargets {
    std::optional<VoiceTargets> left;
    std::optional<VoiceTargets> right;
};

/* The driver, a pure function of the snapshot: silent outside a match,
 * while paused, before GO, while you are out, and with no floor below
 * you. */
LedgeTargets ledge_warning(const A11yMatchState& match, const LedgeMapping& mapping);

}  // namespace a11y
