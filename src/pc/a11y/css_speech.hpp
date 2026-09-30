/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select speech: reads character select aloud from a snapshot of
 * its state taken once a frame, following the local player's hand
 * (.scratch/character-select/spec.md, "Stage 1: announcements"). The screen
 * opening says the player and their character; a coin the hand carries says
 * each portrait it enters, and being picked up (with the portrait under it),
 * cleared or put back on its earlier choice; the costume changing says its
 * number. A choice is silent: the game's announcer names the character. It
 * compares each snapshot with the previous one and touches no game state
 * itself. */
#pragma once
#include "game_access.h"
#include <set>
#include <string>

namespace a11y {

class Speech;

class CssSpeech {
public:
    /* speech must outlive this. */
    explicit CssSpeech(Speech& speech) : m_speech(speech) {}

    /* A scene was entered: the next snapshot is an arrival, spoken as the
     * screen opening. */
    void forget() {
        m_seen = false;
        m_pickup_pending = false;
    }

    /* The screen's state this frame. Speaks what the local hand changed since
     * the last one, interrupting; nothing when nothing did. */
    void frame(const A11yCssState& state);

private:
    std::string opening(const A11yCssState& state);
    /* The announcement for what changed from last to now, or empty. */
    std::string change_announcement(const A11yCssState& last, const A11yCssState& now);
    /* The local hand holds the coin of slot coin, over the portrait now shows
     * under it: "Holding your coin. Fox". */
    std::string holding(const A11yCssState& now, int coin);
    /* The local hand let go of the coin of slot coin. */
    std::string dropped(const A11yCssState& last, const A11yCssState& now, int coin);
    /* A character's name; a missing one is spoken by number and logged. */
    std::string name(A11yCharacter character);

    Speech& m_speech;
    bool m_seen = false;
    /* A coin was picked up last frame and is not announced yet. */
    bool m_pickup_pending = false;
    A11yCssState m_last{};
    /* The characters missing from the names table that were logged. */
    std::set<A11yCharacter> m_logged_missing;
};

}  // namespace a11y
