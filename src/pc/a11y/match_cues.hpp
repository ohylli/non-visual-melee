/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cues of a match (primer docs/a11y/cues.md, "The match cues"): each
 * registers its values with the cue settings and its voices with the cue
 * engine once, then turns every tick's match snapshot into its voices'
 * targets through its driver. Game thread only. */
#pragma once
#include "cue_engine.hpp"
#include "cue_settings.hpp"
#include "gap_warning.hpp"
#include "game_access.h"
#include "ledge_warning.hpp"
#include "position_cue.hpp"
#include <optional>

namespace a11y {

class MatchCues {
public:
    /* The shortest fast interval and the most octaves down the settings
     * allow (half the widest pitch range), which your cue's tick is made to
     * fit: the tick played an octave down must end inside the fastest
     * pulse. */
    static constexpr float kFastestInterval = 0.07f;
    static constexpr float kWidestDownRange = 1.0f;
    /* The shortest interval the opponent's cue allows, which its longer
     * pulse fits the same way. */
    static constexpr float kOpponentFastestInterval = 0.2f;

    /* Before the cue settings file is read, so the file gains these
     * values. */
    MatchCues(CueSettings& settings, CueEngine& engine);

    /* After every simulated tick, not while rollback re-runs ticks: starts,
     * updates or stops each voice. */
    void tick(const A11yMatchState& match);

    /* The voices' numbers in the engine, -1 for one it refused. */
    int your_voice() const { return m_your_voice; }
    int opponent_voice() const { return m_opponent_voice; }
    int gap_voice() const { return m_gap_voice; }
    int ledge_left_voice() const { return m_ledge_left_voice; }
    int ledge_right_voice() const { return m_ledge_right_voice; }

    /* How far past each end of the floor below you the snapshot's deadly
     * test casts (A11yFloorEnd), as the ledge warning's settings hold it. */
    float edge_offset() const { return m_ledge.edge_offset.value(); }

private:
    struct StageValues {
        Switch pan_to_camera;
        Number pitch_range;

        StageMapping mapping() const;
    };
    struct YourValues {
        StageValues stage;
        Number slow_interval;
        Number fast_interval;
        Number far_distance;
        Number near_distance;

        YourMapping mapping() const;
    };
    struct OpponentValues {
        StageValues stage;
        Number interval;

        OpponentMapping mapping() const;
    };
    struct GapValues {
        Number hold;

        GapMapping mapping() const;
    };
    struct LedgeValues {
        Number distance;
        Number pan_floor;
        Number level_floor;
        Number edge_offset;

        LedgeMapping mapping() const;
    };

    static StageValues add_stage_values(ValueGroup group);
    static YourValues add_your_values(ValueGroup group);
    static OpponentValues add_opponent_values(ValueGroup group);
    static GapValues add_gap_values(ValueGroup group);
    static LedgeValues add_ledge_values(ValueGroup group);
    /* A voice with targets starts, or only moves when it plays; one without
     * stops. */
    void play(int voice, const std::optional<VoiceTargets>& targets);

    CueEngine& m_engine;
    /* In the order the cue settings file lists them: each cue registers its
     * group, then its values. */
    CueGroup m_your_cue;
    YourValues m_your;
    CueGroup m_opponent_cue;
    OpponentValues m_opponent;
    CueGroup m_gap_cue;
    GapValues m_gap;
    GapWarning m_gap_warning;
    CueGroup m_ledge_cue;
    LedgeValues m_ledge;
    int m_your_voice;
    int m_opponent_voice;
    int m_gap_voice;
    int m_ledge_left_voice;
    int m_ledge_right_voice;
    int m_tick = 0;
};

}  // namespace a11y
