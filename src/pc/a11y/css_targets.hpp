/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's targets: what a hand would act on where it is
 * (.scratch/character-select/spec.md, "Targets"), and where a step goes from
 * it ("Stage 2: stepping"). The game keeps no "hovered button" anywhere; it
 * tests the hand's position against each target when A is pressed, so this
 * repeats those tests on a snapshot, with the game's rectangles, distances
 * and order. Pure: it touches no game state. */
#pragma once
#include "game_access.h"
#include "steering.hpp"

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

/* A D-pad direction. */
enum class Direction { left, right, up, down };

/* Where the hand goes to be on a target: for a portrait, where the coin sits
 * at the portrait's centre. */
Point aim_point(const A11yCssState& state, Target target);

/* Where a step goes. from is the target the hand is on, or on its way to;
 * (x, y) is the hand. From a portrait: Left and Right to the next unlocked
 * portrait in its row, Up and Down to the unlocked portrait nearest in x in
 * the next row that has one; from itself at an edge. From anywhere else, the
 * nearest target that way by the same rows (Up and Down to the nearest row,
 * Left and Right along the row the hand is nearest to), or none. Portraits
 * only so far: the top bar and the player slots come with their steps. */
Target step(const A11yCssState& state, Target from, float x, float y, Direction direction);

}  // namespace a11y
