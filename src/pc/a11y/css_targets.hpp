/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's targets: what a hand would act on where it is
 * (.scratch/character-select/spec.md, "Targets"). The game keeps no "hovered
 * button" anywhere; it tests the hand's position against each target when A
 * is pressed, so this repeats those tests on a snapshot, with the game's
 * rectangles, distances and order. Pure: it touches no game state. */
#pragma once
#include "game_access.h"

namespace a11y {

enum class TargetKind {
    none,
    portrait,
    /* A player slot's HMN/CPU button, its team button, its sliders' knobs,
     * and the local player's name box. */
    slot_button,
    team_button,
    cpu_level,
    handicap,
    name_box,
    /* The top bar. */
    teams,
    rules,
    back,
};

struct Target {
    TargetKind kind = TargetKind::none;
    /* The portrait's number or the player slot's; -1 for the top bar. */
    int index = -1;

    bool operator==(const Target&) const = default;
};

/* Where a carried coin sits from the hand; portraits are tested there. */
inline constexpr float kCoinOffsetX = 2.7f;
inline constexpr float kCoinOffsetY = -2.0f;

/* The target the local player's hand would reach at (x, y). Carrying a coin,
 * the portrait under the coin: a carrying hand presses no button. Free, the
 * button or knob A would act on, first in the game's order, or else the
 * portrait a coin would be over. None over empty space, over a locked
 * portrait, while the hand holds a slider, and while the player's name tag
 * window is open. */
Target target_at(const A11yCssState& state, float x, float y);

}  // namespace a11y
