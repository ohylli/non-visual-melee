/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What the match cues' drivers ask of the match snapshot alike. */
#pragma once
#include "game_access.h"

namespace a11y {

/* You are in play: a match is running and not paused, past GO, and you are
 * neither knocked out nor waiting to respawn. Every cue about you is silent
 * otherwise. */
inline bool you_in_play(const A11yMatchState& match) {
    const A11yMatchFighter& you = match.you;
    return match.running && !match.paused && you.can_move && !you.out;
}

}  // namespace a11y
