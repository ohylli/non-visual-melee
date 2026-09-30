/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_targets.hpp"

namespace a11y {
namespace {

/* The top bar's buttons, as mnCharSel_CursorThink tests them: above y 22,
 * Back right of x 17.3, the rules header between x -17 and 15, Teams left of
 * x -25.5. */
constexpr float kTopBarBottom = 22.0f;
constexpr float kBackLeft = 17.3f;
constexpr float kRulesLeft = -17.0f;
constexpr float kRulesRight = 15.0f;
constexpr float kTeamsRight = -25.5f;
/* A free hand grabs a slider's knob within this squared distance. */
constexpr float kGrabDistanceSquared = 5.0f;

bool inside(const A11yCssRect& rect, float x, float y) {
    return x > rect.left && x < rect.right && y < rect.top && y > rect.bottom;
}

bool near(const A11yCssKnob& knob, float x, float y) {
    float dx = x - knob.x;
    float dy = y - knob.y;
    return knob.known && dx * dx + dy * dy < kGrabDistanceSquared;
}

Target portrait_at(const A11yCssState& state, float x, float y) {
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        const A11yCssPortrait& portrait = state.portraits[i];
        if (!portrait.locked && inside(portrait.rect, x + kCoinOffsetX, y + kCoinOffsetY)) {
            return Target{TargetKind::portrait, i};
        }
    }
    return Target{};
}

Target top_bar_at(const A11yCssState& state, float x, float y) {
    if (y <= kTopBarBottom) {
        return Target{};
    }
    if (x > kBackLeft) {
        return Target{TargetKind::back};
    }
    if (state.has_rules_button && x > kRulesLeft && x < kRulesRight) {
        return Target{TargetKind::rules};
    }
    if (state.has_teams_button && x < kTeamsRight) {
        return Target{TargetKind::teams};
    }
    return Target{};
}

/* The player slots' buttons in VS modes, slot by slot, each slot's in the
 * order the game tries them; the name box after every slot. */
Target slot_target_at(const A11yCssState& state, float x, float y) {
    for (int i = 0; i < state.slot_count && i < A11Y_CSS_SLOTS; i++) {
        const A11yCssSlot& slot = state.slots[i];
        /* The HMN/CPU button waits while anything of the slot is in a hand,
         * and while its name tag window is open. */
        if (!slot.cpu_level_held && !slot.handicap_held && !slot.carried && !slot.hand_holding &&
            !slot.name_tags_open && inside(slot.slot_button, x, y))
        {
            return Target{TargetKind::slot_button, i};
        }
        if (state.teams && slot.kind != A11Y_CSS_CLOSED && inside(slot.team_button, x, y)) {
            return Target{TargetKind::team_button, i};
        }
        if (!slot.cpu_level_held && slot.kind == A11Y_CSS_CPU && near(slot.cpu_level_knob, x, y)) {
            return Target{TargetKind::cpu_level, i};
        }
        /* A player sets their own handicap; anyone sets a CPU's. */
        if (!slot.handicap_held && state.handicap_sliders && slot.kind != A11Y_CSS_CLOSED &&
            (slot.kind == A11Y_CSS_CPU || i == state.local_slot) && near(slot.handicap_knob, x, y))
        {
            return Target{TargetKind::handicap, i};
        }
    }
    int own = state.local_slot;
    if (own < state.slot_count && state.slots[own].kind == A11Y_CSS_HUMAN &&
        inside(state.slots[own].name_box, x, y))
    {
        return Target{TargetKind::name_box, own};
    }
    return Target{};
}

}  // namespace

Target target_at(const A11yCssState& state, float x, float y) {
    if (state.hand.slider != A11Y_CSS_NO_SLIDER || state.slots[state.local_slot].name_tags_open) {
        return Target{};
    }
    if (state.hand.coin < 0) {
        Target target = top_bar_at(state, x, y);
        /* Single-player modes' own buttons come with their reader. */
        if (target.kind == TargetKind::none && state.hand_count == A11Y_CSS_SLOTS) {
            target = slot_target_at(state, x, y);
        }
        if (target.kind != TargetKind::none) {
            return target;
        }
    }
    return portrait_at(state, x, y);
}

}  // namespace a11y
