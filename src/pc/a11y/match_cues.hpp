/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cues of a match (primer docs/a11y/cues.md, "The match cues"): each
 * registers its values with the cue settings and its voices with the cue
 * engine once, then turns every tick's match snapshot into its voices'
 * targets through its driver. Game thread only. */
#pragma once
#include "cue_engine.hpp"
#include "cue_settings.hpp"
#include "game_access.h"
#include "position_cue.hpp"

namespace a11y {

class MatchCues {
public:
    /* The shortest fast interval and the widest down range the settings
     * allow, which the tick's voice is made to fit: the tick played an
     * octave down must end inside the fastest pulse. */
    static constexpr float kFastestInterval = 0.07f;
    static constexpr float kWidestDownRange = 1.0f;

    /* Before the cue settings file is read, so the file gains these values. */
    MatchCues(CueSettings& settings, CueEngine& engine);

    /* After every simulated tick, not while rollback re-runs ticks: starts,
     * updates or stops each voice. */
    void tick(const A11yMatchState& match);

private:
    struct PositionValues {
        Number edge_pan;
        Number up_range;
        Number down_range;
        Number slow_interval;
        Number fast_interval;
        Number far_distance;
        Number near_distance;

        PositionMapping mapping() const;
    };

    MatchCues(const CueGroup& your, CueEngine& engine);
    static PositionValues add_position_values(ValueGroup group);

    CueEngine& m_engine;
    PositionValues m_your;
    int m_your_voice;
    int m_tick = 0;
};

}  // namespace a11y
