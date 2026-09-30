/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A hand-written snapshot of character select for the tests of its reader
 * and its targets. The rectangles and knobs are those a run reported: the
 * player slots' buttons from the game's table, the knobs and the name box
 * from the model. */
#pragma once
#include "character_kinds.h"
#include "game_access.h"
#include <cmath>

namespace css_test {

/* The characters under the names players know. */
constexpr A11yCharacter kDrMario = A11Y_CKind_DrMario, kMario = A11Y_CKind_Mario,
                        kLuigi = A11Y_CKind_Luigi, kBowser = A11Y_CKind_Koopa,
                        kPeach = A11Y_CKind_Peach, kYoshi = A11Y_CKind_Yoshi,
                        kDk = A11Y_CKind_Donkey, kFalcon = A11Y_CKind_Captain,
                        kGanon = A11Y_CKind_Ganon, kFalco = A11Y_CKind_Falco, kFox = A11Y_CKind_Fox,
                        kNess = A11Y_CKind_Ness, kIceClimbers = A11Y_CKind_PopoNana,
                        kKirby = A11Y_CKind_Kirby, kSamus = A11Y_CKind_Samus,
                        kZelda = A11Y_CKind_Zelda, kLink = A11Y_CKind_Link,
                        kYoungLink = A11Y_CKind_CLink, kPichu = A11Y_CKind_Pichu,
                        kPikachu = A11Y_CKind_Pikachu, kJigglypuff = A11Y_CKind_Purin,
                        kMewtwo = A11Y_CKind_Mewtwo, kGameWatch = A11Y_CKind_GameWatch,
                        kMarth = A11Y_CKind_Mars, kRoy = A11Y_CKind_Emblem;

/* The portraits in the game's table order (icons in mncharsel.c). */
constexpr A11yCharacter kPortraitCharacters[A11Y_CSS_PORTRAITS] = {kDrMario, kMario, kLuigi,
    kBowser, kPeach, kYoshi, kDk, kFalcon, kGanon, kFalco, kFox, kNess, kIceClimbers, kKirby,
    kSamus, kZelda, kLink, kYoungLink, kPichu, kPikachu, kJigglypuff, kMewtwo, kGameWatch, kMarth,
    kRoy};
constexpr int kYoshiPortrait = 5;
constexpr int kFoxPortrait = 10;
constexpr int kNessPortrait = 11;
constexpr int kMarthPortrait = 23;
/* What a slot's portrait holds from the moment its coin is picked up. */
constexpr int kPlaceholder = 0xD;

/* The portraits' columns (ICONBNDS_* in mncharsel.c); rows of 9, 9 and 7,
 * the bottom row starting one column in. */
constexpr float kColumnLefts[] = {
    -30.0f, -24.4f, -17.4f, -10.4f, -3.4f, 3.6f, 10.6f, 17.6f, 24.4f, 30.2f};
constexpr float kRowTops[] = {20.0f, 13.0f, 6.0f, -1.0f};

inline A11yCssRect portrait_rect(int portrait) {
    int row = portrait < 9 ? 0 : portrait < 18 ? 1 : 2;
    int column = row == 2 ? portrait - 18 + 1 : portrait - 9 * row;
    return A11yCssRect{
        kColumnLefts[column], kColumnLefts[column + 1], kRowTops[row], kRowTops[row + 1]};
}

/* The CPU level knob of a CPU's slot at a level; one level is 1.25 wide, and
 * the slider's lowest end is where the knob is at level 1. */
inline A11yCssKnob cpu_level_knob(int slot, int level) {
    float origin = -15.5f + 15.4f * static_cast<float>(slot - 1);
    return A11yCssKnob{true, origin + 1.25f * static_cast<float>(level - 1), -15.12f, origin};
}

/* Where the game puts a coin on a portrait it chooses for a slot, as a new
 * CPU's (mnCharSel_8025FB50) or one put back (mnCharSel_8025FDEC): this far
 * right of the portrait's left edge and below its top. */
constexpr float kCoinRestRight = 3.4f;
constexpr float kCoinRestDown = 3.0f;

inline A11yCssCoinReport coin_at_rest(int portrait) {
    A11yCssRect rect = portrait_rect(portrait);
    return A11yCssCoinReport{true, rect.left + kCoinRestRight, rect.top - kCoinRestDown};
}

/* The value a held slider shows with the hand at x, the slider's lowest end
 * at origin (updateGrabbedSlider): the hand's place along the slider's 10
 * units, times 0.8, plus 0.5, truncated, plus 1. */
inline int slider_value(float origin, float x) {
    float along = std::fmin(std::fmax(x - origin, 0.0f), 10.0f);
    return static_cast<int>(0.8f * along + 0.5f) + 1;
}

inline A11yCssSlot slot(A11yCssSlotKind kind, int index) {
    A11yCssSlot out{};
    out.kind = kind;
    out.portrait = -1;
    out.over_portrait = -1;
    out.character = A11Y_NO_CHARACTER;
    out.cpu_level = 1;
    out.handicap = 9;
    float step = 15.4f * static_cast<float>(index);
    out.slot_button = A11yCssRect{-35.6f + step, -28.6f + step, 0.2f, -4.6f};
    out.team_button = A11yCssRect{-26.8f + step, -21.0f + step, -1.0f, -5.8f};
    /* Hidden in the slot until it is a CPU's. */
    out.cpu_level_knob = A11yCssKnob{true, -30.9f + step, -20.12f, -30.9f + step};
    out.handicap_knob = out.cpu_level_knob;
    return out;
}

/* A slot made a CPU with a character resting on its portrait. */
inline A11yCssSlot cpu(int index, int portrait) {
    A11yCssSlot out = slot(A11Y_CSS_CPU, index);
    out.portrait = portrait;
    out.over_portrait = portrait;
    out.character = kPortraitCharacters[portrait];
    out.cpu_level_knob = cpu_level_knob(index, 1);
    out.coin = coin_at_rest(portrait);
    return out;
}

/* VS Melee, player 1 at home with no character and a free hand, player 2 a
 * CPU with Yoshi, the others closed; every portrait unlocked. */
inline A11yCssState vs_screen() {
    A11yCssState state{};
    state.hand_count = A11Y_CSS_SLOTS;
    state.slot_count = A11Y_CSS_SLOTS;
    state.has_teams_button = true;
    state.has_rules_button = true;
    state.hand = A11yCssHand{true, -1, A11Y_CSS_NO_SLIDER, -1, -31.0f, -21.5f};
    for (int i = 0; i < A11Y_CSS_SLOTS; i++) {
        state.slots[i] = slot(A11Y_CSS_CLOSED, i);
    }
    state.slots[0].kind = A11Y_CSS_HUMAN;
    state.slots[0].name_box = A11yCssRect{-32.7f, -22.8f, -17.0f, -20.0f};
    state.slots[1] = cpu(1, kYoshiPortrait);
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        state.portraits[i].character = kPortraitCharacters[i];
        state.portraits[i].rect = portrait_rect(i);
    }
    return state;
}

}  // namespace css_test
