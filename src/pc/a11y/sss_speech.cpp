/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sss_speech.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include "sss_names.hpp"
#include <optional>
#include <string_view>

namespace a11y {
namespace {

/* Appends one part of an announcement as its own sentence. */
void add(std::string& out, std::string_view part) {
    if (part.empty()) {
        return;
    }
    if (!out.empty()) {
        out += out.back() == '.' ? " " : ". ";
    }
    out += part;
}

bool is_tile(int tile) {
    return tile >= 0 && tile < A11Y_SSS_TILES;
}

/* A pick on this tile rolls a stage at random: the random tile, or Start
 * before the stage cursor has entered any. */
bool rolls(int tile) {
    return tile == A11Y_SSS_RANDOM_TILE || tile == A11Y_SSS_NO_TILE;
}

}  // namespace

void SssSpeech::frame(const A11ySssState& state, const GlideState<int>& glide) {
    bool arrived = !m_seen;
    A11ySssState last = m_last;
    m_seen = true;
    m_last = state;
    if (state.skipped) {
        /* The screen is never shown: the stage chosen in advance stands in
         * for the opening. */
        if (arrived) {
            m_speech.announce(
                std::string(kStage) + ": " + name(state.skipped_stage) + ".", Mode::interrupt);
        }
        return;
    }
    if (arrived) {
        m_speech.announce(opening(state), Mode::interrupt);
        return;
    }

    std::string out;
    /* A pick acts on the tile hovered before its frame's cursor movement:
     * the last snapshot's. */
    bool picked = last.pick == A11Y_SSS_CHOOSING && state.pick != A11Y_SSS_CHOOSING;
    if (!state.online && picked && rolls(last.hovered) && is_tile(state.hovered)) {
        /* Offline the roll lands in the hovered index at once; online it
         * waits for the other player's pick. */
        add(out, std::string(kRandom) + ": " + tile_words(state, state.hovered) + ".");
    } else if (picked && !rolls(last.hovered) && last.hovered != m_named) {
        /* A during a glide picks the tile crossed, not the one the step
         * named. */
        add(out, std::string(kPicked) + " " + tile_words(state, last.hovered) + ".");
    }
    if (state.online && picked) {
        /* The other player's pick stays unsaid until the coin flip. */
        add(out, kWaitingForOpponent);
    }
    bool pressed = state.pressed_a || state.pressed_start;
    if (!state.online && pressed && state.pick == A11Y_SSS_CHOOSING &&
        last.hovered < A11Y_SSS_RANDOM_TILE && state.tiles[last.hovered].state == A11Y_SSS_LOCKED)
    {
        add(out, kLocked);
    }
    /* Once a pick is made the stage cursor is hidden, and the hovered index
     * holds what was picked. During a glide, and on the frame it has ended,
     * what the cursor crosses goes unsaid: the step named where it goes. */
    bool gliding = glide.phase == GlidePhase::gliding;
    bool glide_ended = glide.phase == GlidePhase::ended || glide.phase == GlidePhase::failed;
    if (state.pick == A11Y_SSS_CHOOSING && glide_ended) {
        if (glide.phase == GlidePhase::failed) {
            add(out, std::string(kCouldNotReach) + " " + tile_words(state, glide.destination));
        }
        if (state.hovered != glide.destination && is_tile(state.hovered)) {
            add(out, hover_words(state, state.hovered));
            m_named = state.hovered;
        }
    } else if (state.pick == A11Y_SSS_CHOOSING && !gliding && state.hovered != last.hovered &&
               is_tile(state.hovered))
    {
        add(out, hover_words(state, state.hovered));
        m_named = state.hovered;
    }
    if (!out.empty()) {
        m_speech.announce(out, Mode::interrupt);
    }
}

void SssSpeech::step(const A11ySssState& state, int tile) {
    if (!is_tile(tile)) {
        return;
    }
    m_named = tile;
    m_speech.announce(hover_words(state, tile), Mode::interrupt);
}

void SssSpeech::resolved(
    const A11ySssState& state, int stage_tile, int local_pick, int remote_pick) {
    if (!state.online || m_resolved || !is_tile(stage_tile)) {
        return;
    }
    m_resolved = true;
    m_speech.announce(result(state, stage_tile, local_pick, remote_pick), Mode::interrupt);
}

std::string SssSpeech::tile_words(const A11ySssState& state, int tile) {
    if (tile == A11Y_SSS_RANDOM_TILE) {
        return std::string(kRandom);
    }
    if (state.tiles[tile].state == A11Y_SSS_LOCKED) {
        return std::string(kLockedStage);
    }
    return name(state.tiles[tile].stage);
}

std::string SssSpeech::hover_words(const A11ySssState& state, int tile) {
    std::string out = tile_words(state, tile);
    if (tile == A11Y_SSS_RANDOM_TILE || state.tiles[tile].state == A11Y_SSS_LOCKED) {
        return out;
    }
    if (std::optional<std::string_view> description = stage_description(state.tiles[tile].stage)) {
        out += ". " + std::string(*description);
    }
    return out;
}

std::string SssSpeech::opening(const A11ySssState& state) {
    std::string out = std::string(kStageSelect) + ". " + std::string(kHowToChoose);
    /* Steering writes controller 1's pad only. Online the stage cursor is
     * always the local player's, on controller 1. */
    if (!state.online && state.owner > 0) {
        out += " " + std::string(kPlayer) + " " + std::to_string(state.owner + 1) + " " +
               std::string(kChooses) + ". " + std::string(kSteeringNeedsPort1);
    }
    return out;
}

std::string SssSpeech::result(
    const A11ySssState& state, int stage_tile, int local_pick, int remote_pick) {
    std::string stage = tile_words(state, stage_tile);
    std::string random = std::string(kRandom) + ": ";
    bool local_random = rolls(local_pick);
    bool remote_random = rolls(remote_pick);
    if (local_random && remote_random) {
        return random + stage + ".";
    }
    if (local_pick == remote_pick) {
        return stage + ".";
    }
    /* A named pick that names the stage won, though a Random beside it may
     * have rolled the same stage: either way the stage is the one named. */
    if (!local_random && stage_tile == local_pick) {
        return stage + ", " + std::string(kYourPick) + ".";
    }
    if (!remote_random && stage_tile == remote_pick) {
        return stage + ", " + std::string(kOpponentsPick) + ".";
    }
    if (local_random) {
        return random + stage + ", " + std::string(kYourPick) + ".";
    }
    if (remote_random) {
        return random + stage + ", " + std::string(kOpponentsPick) + ".";
    }
    return stage + ".";
}

std::string SssSpeech::name(A11yStage stage) {
    if (std::optional<std::string_view> known = stage_name(stage)) {
        return std::string(*known);
    }
    if (m_logged_missing.insert(stage).second && log_enabled()) {
        pc_log_line("[a11y] stage %d: not in the stage names table", stage);
    }
    return std::string(kUnknownStage) + " " + std::to_string(stage);
}

}  // namespace a11y
