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
#include <optional>
#include <vector>

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
    /* A single-player mode's arrow. */
    arrow,
};

struct Target {
    TargetKind kind = TargetKind::none;
    /* The portrait's number, the player slot's or the arrow's in kArrows; -1
     * for the top bar. */
    int index = -1;
    /* A value of the slider the hand holds, 1 to 9, as a step's destination
     * or where the hand is; none for everything else, the knob included. */
    std::optional<int> level;

    bool operator==(const Target&) const = default;
};

/* The part of the screen a target is in; none for no target. */
enum class Area { none, top_bar, portraits, player_slots };
Area area(Target target);

/* A player slot's slider: what the hand holds, the knob it is reached at, the
 * knob's place in the slot, and the value it sets. */
struct SliderKind {
    A11yCssSlider slider;
    TargetKind knob_kind;
    A11yCssKnob A11yCssSlot::* knob;
    int A11yCssSlot::* value;
};

/* The slider held as slider, or reached at a knob of kind knob; nullptr for
 * none. */
const SliderKind* slider_kind(A11yCssSlider slider);
const SliderKind* slider_kind(TargetKind knob);

/* A row of a single-player mode's arrows: its place in A11yCssArrows. */
using ArrowRow = A11yCssArrowRow A11yCssArrows::*;

/* A single-player mode's arrow: its row, and whether A on it steps the row's
 * value up. */
struct ArrowKind {
    ArrowRow row;
    bool higher;
};

/* The arrows by their numbers, as drawn, row by row, each left to right: the
 * difficulty's lower and higher, then the stock count's fewer and more. */
inline constexpr ArrowKind kArrows[] = {
    {&A11yCssArrows::difficulty, false},
    {&A11yCssArrows::difficulty, true},
    {&A11yCssArrows::stocks, false},
    {&A11yCssArrows::stocks, true},
};

/* The arrow target's kind; the target must be an arrow. */
const ArrowKind& arrow_kind(Target arrow);

/* The screen is a single-player mode's: one hand, and arrows where VS modes
 * have the player slots' buttons. */
bool single_player(const A11yCssState& state);

/* Where a carried coin sits from the hand; portraits are tested there. */
inline constexpr float kCoinOffsetX = 2.7f;
inline constexpr float kCoinOffsetY = -2.0f;

/* A slider's values. */
inline constexpr int kLowestLevel = 1;
inline constexpr int kHighestLevel = 9;

/* The target the local player's hand would reach at (x, y). Carrying a coin,
 * the portrait under the coin: a carrying hand presses no button. Free, the
 * button or knob A would act on, first in the game's order, or else the
 * portrait a coin would be over. None over empty space, over a locked
 * portrait, while the hand holds a slider, and while the player's name tag
 * window is open. */
Target target_at(const A11yCssState& state, float x, float y);

/* A D-pad direction. */
enum class Direction { left, right, up, down };

/* The targets a step reaches, in rows top to bottom, each left to right: the
 * top bar, the rows of unlocked portraits as drawn, and in VS modes the
 * player slots, each slot's HMN/CPU button, then its team button, CPU level
 * knob and handicap knob where they exist; in single-player modes the
 * difficulty arrows and below them the stock arrows, where shown. Empty rows
 * are left out. The name box is not among them. */
std::vector<std::vector<Target>> target_rows(const A11yCssState& state);

/* What among the rows the hand at (x, y) is on, by the areas the game tests,
 * whether or not it would react now (a hand carrying a coin, a slot whose
 * coin another hand carries): a portrait under where the coin sits, a button,
 * a knob within reach. A hand holding a slider is on its value. None
 * elsewhere. */
Target locate(const A11yCssState& state, float x, float y);

/* The player slot whose coin rests on the portrait where the local hand, free
 * of anything, may pick it up (its own or a CPU's), a CPU's before its own:
 * B calls one's own back from anywhere. -1 for none, and while the hand holds
 * anything. */
int pickable_coin(const A11yCssState& state, int portrait);

/* Where the hand goes to be on a target. For a portrait, where the coin sits
 * at the portrait's centre; for a free hand, where A picks up the coin
 * resting there, if pickable_coin names one. For a button, its middle, the
 * height A leaves the hand at; for a knob, the point it is grabbed from; for
 * a value of the held slider, the middle of that value's span. */
Point aim_point(const A11yCssState& state, Target target);

/* A on the arrow would change nothing: its value is at that end of the
 * range. */
bool arrow_at_end(const A11yCssState& state, Target arrow);

/* Where a step goes. from is where the hand is going, or is (locate()).
 * Along its row to the next target, Up and Down to the target nearest in x
 * in the next row; from itself at an edge. A hand holding a slider steps its
 * value with Left and Right, and Up and Down keep it where it is. From
 * anything not in the rows, as from where the hand is; from anywhere else,
 * the nearest target that way: Up and Down to the nearest row that way,
 * Left and Right along the row the hand is nearest to, else the nearest
 * target that way in any row. None when nothing lies that way. */
Target step(const A11yCssState& state, Target from, float x, float y, Direction direction);

}  // namespace a11y
