/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Pieces of hand-written match snapshots for the tests of the match cues:
 * Final Destination's frame as measured (docs/a11y/gameplay-state.md,
 * "Sizes measured") and a fighter in play. */
#pragma once
#include "game_access.h"

namespace match_test {

inline A11yStageFrame final_destination() {
    A11yStageFrame stage{};
    stage.blast_left = -246.0f;
    stage.blast_right = 246.0f;
    stage.blast_top = 188.0f;
    stage.blast_bottom = -140.0f;
    stage.camera_left = -170.0f;
    stage.camera_right = 170.0f;
    stage.camera_top = 114.0f;
    stage.camera_bottom = -80.0f;
    return stage;
}

/* In play at (x, y), its input on. */
inline A11yMatchFighter fighter(int slot, float x, float y) {
    A11yMatchFighter f{};
    f.slot = slot;
    f.x = x;
    f.y = y;
    f.can_move = true;
    return f;
}

}  // namespace match_test
