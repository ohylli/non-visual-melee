/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The position cues' drivers (primer docs/a11y/cues.md, "The match cues"):
 * pure functions from the match snapshot and a cue's mapping values to its
 * voice's targets. Pan says where across the stage and pitch how high, the
 * same way for both cues; your cue's pulse speed says how near the opponent
 * is, and the opponent's pulse is steady. The opponent's cue can instead
 * say where the opponent is from you. */
#pragma once
#include "cue_engine.hpp"
#include "game_access.h"
#include <optional>

namespace a11y {

/* Where on the stage, as pan and pitch: the mapping both position cues
 * use, each with its own copy of the pitch range. */
struct StageMapping {
    /* Pan framed by the camera box's sides rather than the side blast
     * zones: more of the pan for the stage itself, none for the last
     * stretch before a knockout. One switch sets it for both cues, so their
     * pans stay comparable. */
    bool pan_to_camera = true;
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
    /* Where the opponent is from you rather than on the stage: pan from
     * the horizontal distance, pitch from the vertical, the stage mapping's
     * pitch range kept and its pan frame left aside. */
    bool relative = true;
    /* Relative only. World units from you to the opponent's side where the
     * pan is full, the pan level with you, so the side is always heard,
     * how far past you the opponent goes before the pulse changes sides,
     * and how far above or below you the pitch reaches the range's ends. */
    float side_distance = 80.0f;
    float pan_floor = 0.2f;
    float crossing_margin = 4.0f;
    float height_distance = 60.0f;
};

/* Pan for an x, linear from the left blast zone, -1 and beyond, to the
 * right one, 1 and beyond, or with to_camera from the camera box's left
 * side to its right. No stage's floor is a reference, so every stage maps
 * the same way and a fall past the floor's end keeps moving the pan, up to
 * the frame's side. */
float stage_pan(const A11yStageFrame& stage, float x, bool to_camera);

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

/* The opponent's side of you for a horizontal distance (opponent minus
 * you): -1 left, 1 right. The last side, 0 for none, holds until the
 * opponent is more than the margin past you the other way; with none, the
 * distance's sign decides, level with you counting as the right. */
int opponent_side(float dx, int last_side, float margin);

/* The relative pan for a horizontal distance on a side: the floor level
 * with you, linear in the distance to full at the side distance and
 * beyond. */
float relative_pan(float dx, int side, const OpponentMapping& mapping);

/* The relative pitch as playback rate for a vertical distance (opponent
 * minus you): linear in octaves, the cue's own pitch level with you,
 * pitch_range/2 octaves up at the height distance above and beyond, as
 * many down as far below. */
float relative_pitch(float dy, const OpponentMapping& mapping);

/* The opponent's position cue's driver: in the stage mode a function of
 * the snapshot alone; relative to you it keeps the opponent's side, its
 * only state, so that a crossing flips the pulse once. */
class OpponentPosition {
public:
    /* Once a tick: the voice's targets, or none while it is silent:
     * outside a match, paused, before GO, with no opponent and while the
     * opponent is knocked out or waiting to respawn; relative to you, also
     * while you are, since your place is then where you were knocked out.
     * Each tick follows the opponent the snapshot chose, so a change of
     * opponent only moves it; relative to you a new opponent, or one back
     * from a silence, takes their side afresh. */
    std::optional<VoiceTargets> tick(const A11yMatchState& match, const OpponentMapping& mapping);
    /* The side forgotten: the next tick decides from its snapshot alone. */
    void forget() { m_side = 0; }

private:
    int m_side = 0;  /* -1 left, 1 right, 0 none yet */
    int m_slot = -1; /* whose side it is */
};

/* The position cues' previews (cue_preview.hpp), for the seconds since the
 * preview began, none once it has ended: the fighter at the centre at
 * middle height, so centred at the cue's own pitch, and the opponent far
 * off, so your cue pulses at its slow interval and the opponent's at its
 * own, for about kPreviewSeconds. Relative to you the opponent is level
 * with you instead, half the pulses on your left and the rest on your
 * right, so both sides of the pan floor are heard. */
std::optional<VoiceTargets> your_position_preview(double seconds, const YourMapping& mapping);
std::optional<VoiceTargets> opponent_position_preview(
    double seconds, const OpponentMapping& mapping);

}  // namespace a11y
