/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The position cues' drivers (primer docs/a11y/cues.md, "The match cues"):
 * pure functions from the match snapshot and a cue's mapping values to its
 * voice's targets. Pan says where across the stage and pitch how high, the
 * same way for both cues; your cue's pulse speed says how near the opponent
 * is, and the opponent's pulse is steady. */
#pragma once
#include "cue_engine.hpp"
#include "game_access.h"
#include <optional>

namespace a11y {

/* Where on the stage, as pan and pitch: the mapping both position cues
 * use, each with its own copy of the values. */
struct StageMapping {
    /* Pan at the main stage's edges; the side blast zones are fully left
     * and right. */
    float edge_pan = 0.6f;
    /* Octaves up at the camera box's top, and down at the bottom blast
     * zone, from the main floor. */
    float up_range = 1.0f;
    float down_range = 1.0f;
};

/* Your position cue's mapping, as its settings hold it. */
struct PositionMapping {
    StageMapping stage;
    /* Seconds between pulses with the opponent at or beyond the far
     * distance, or none, and at or within the near distance (world
     * units). */
    float slow_interval = 0.8f;
    float fast_interval = 0.08f;
    float far_distance = 120.0f;
    float near_distance = 10.0f;
};

/* The opponent's position cue's mapping, as its settings hold it. */
struct OpponentMapping {
    StageMapping stage;
    /* Seconds between pulses, whatever the distance: your cue already says
     * how near. */
    float interval = 0.4f;
};

/* Pan for an x, piecewise linear: the main floor's centre 0, its ends
 * ±edge_pan, the side blast zones ±1 and beyond. */
float stage_pan(const A11yStageFrame& stage, float x, float edge_pan);

/* Pitch as playback rate for a y, piecewise linear in octaves: the main
 * floor 1, the camera box's top up_range octaves up and above, the bottom
 * blast zone down_range octaves down and below. */
float stage_pitch(const A11yStageFrame& stage, float y, float up_range, float down_range);

/* A fighter's place on the stage as a voice's pan and pitch, at full gain;
 * the interval is the caller's. */
VoiceTargets stage_targets(
    const A11yStageFrame& stage, const A11yMatchFighter& fighter, const StageMapping& mapping);

/* The pulse interval for the distance to the opponent (none without one):
 * interpolated in the log of the interval, so each unit of distance closed
 * speeds the pulse by the same ratio, from the slow interval at the far
 * distance to the fast one at the near distance. */
float nearness_interval(std::optional<float> distance, const PositionMapping& mapping);

/* Your position cue's targets, or none while it is silent: outside a
 * match, paused, before GO and while you are knocked out or waiting to
 * respawn. */
std::optional<VoiceTargets> your_position(
    const A11yMatchState& match, const PositionMapping& mapping);

/* The opponent's position cue's targets, or none while it is silent:
 * outside a match, paused, before GO, with no opponent and while the
 * opponent is knocked out or waiting to respawn. Each tick follows the
 * opponent the snapshot chose, so a change of opponent only moves it. */
std::optional<VoiceTargets> opponent_position(
    const A11yMatchState& match, const OpponentMapping& mapping);

}  // namespace a11y
