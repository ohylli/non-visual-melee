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
    /* Octaves from the bottom blast zone to the camera box's top, half of
     * them down and half up from the playback rate 1 midway. */
    float pitch_range = 2.0f;
};

/* Your position cue's mapping, as its settings hold it. */
struct YourMapping {
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

/* Pan for an x, linear from the left blast zone, -1 and beyond, to the
 * right one, 1 and beyond. No stage's floor is a reference, so every stage
 * maps the same way and a fall past the floor's end keeps moving the pan. */
float stage_pan(const A11yStageFrame& stage, float x);

/* Pitch as playback rate for a y, linear in octaves from the bottom blast
 * zone, range/2 octaves down and below, to the camera box's top, range/2
 * up and above. The bottom is the blast zone rather than the camera box's,
 * so a fall goes on sounding lower until the knockout. */
float stage_pitch(const A11yStageFrame& stage, float y, float range);

/* A fighter's place on the stage as a voice's pan and pitch, at full gain;
 * the interval is the caller's. */
VoiceTargets stage_targets(
    const A11yStageFrame& stage, const A11yMatchFighter& fighter, const StageMapping& mapping);

/* The pulse interval for the distance to the opponent (none without one):
 * interpolated in the log of the interval, so each unit of distance closed
 * speeds the pulse by the same ratio, from the slow interval at the far
 * distance to the fast one at the near distance. */
float nearness_interval(std::optional<float> distance, const YourMapping& mapping);

/* Your position cue's targets, or none while it is silent: outside a
 * match, paused, before GO and while you are knocked out or waiting to
 * respawn. */
std::optional<VoiceTargets> your_position(const A11yMatchState& match, const YourMapping& mapping);

/* The opponent's position cue's targets, or none while it is silent:
 * outside a match, paused, before GO, with no opponent and while the
 * opponent is knocked out or waiting to respawn. Each tick follows the
 * opponent the snapshot chose, so a change of opponent only moves it. */
std::optional<VoiceTargets> opponent_position(
    const A11yMatchState& match, const OpponentMapping& mapping);

}  // namespace a11y
