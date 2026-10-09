/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage select speech: reads stage select aloud from a snapshot of its state
 * taken once a frame (.scratch/stage-select/spec.md, "Speech"). The opening
 * says the screen and how to choose, and whose controller the stage cursor
 * answers to when it is not controller 1's. Each tile the stage cursor
 * enters says its stage and the stage's description, "Random" or "Locked
 * stage"; crossing empty space is silent. Offline, Random says the stage it
 * rolled, and a pick on a locked tile that the game refuses says so; a pick
 * of a named stage is silent, since its name was just heard. With the
 * stage-select rule Random or Ordered the screen is skipped, and the stage
 * chosen in advance is said instead of the opening. A D-pad step names its
 * destination at the press; what the glide crosses goes unsaid, and so does
 * its arrival, while a glide ending elsewhere names where the cursor is, and
 * a failed one says so. A pick of another stage than the one last named (A
 * during a glide) names the stage picked. It compares each snapshot with the
 * previous one and touches no game state itself. */
#pragma once
#include "game_access.h"
#include "steerer.hpp"
#include <set>
#include <string>

namespace a11y {

class Speech;

class SssSpeech {
public:
    /* speech must outlive this. */
    explicit SssSpeech(Speech& speech) : m_speech(speech) {}

    /* A scene was entered: the next snapshot is an arrival, spoken as the
     * screen opening. */
    void forget() {
        m_seen = false;
        m_named = A11Y_SSS_NO_TILE;
    }

    /* The screen's state this frame, and steering's glide as it stands.
     * Speaks what changed since the last one, interrupting; nothing when
     * nothing did. */
    void frame(const A11ySssState& state, const GlideState<int>& glide = {});
    /* A D-pad step goes to the tile, or at an edge stays on it: its hover
     * words, at the press. */
    void step(const A11ySssState& state, int tile);

    /* A tile as named: "Corneria", "Random", "Locked stage". */
    std::string tile_words(const A11ySssState& state, int tile);
    /* A tile as spoken on entering it: its words, then a stage's
     * description ("Corneria. A fight on the hull of ..."). */
    std::string hover_words(const A11ySssState& state, int tile);

private:
    std::string opening(const A11ySssState& state);
    /* A stage's name; a missing one is spoken by number and logged. */
    std::string name(A11yStage stage);

    Speech& m_speech;
    bool m_seen = false;
    A11ySssState m_last{};
    /* The tile last named by a hover or a step: a pick of another one was
     * made where the player did not hear. */
    int m_named = A11Y_SSS_NO_TILE;
    /* The stages missing from the names table that were logged. */
    std::set<A11yStage> m_logged_missing;
};

}  // namespace a11y
