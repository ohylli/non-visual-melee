/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select speech: reads character select aloud from a snapshot of
 * its state taken once a frame, following the local player's hand
 * (.scratch/character-select/spec.md, "Stage 1: announcements"). The screen
 * opening says the player and their character; a coin the hand carries says
 * each portrait it enters, and being picked up (with the portrait under it),
 * cleared or put back on its earlier choice; the costume changing says its
 * number. A free hand says each button or slider knob it reaches, a slider
 * says being grabbed, its value and being let go, and a player slot or team
 * the hand changes says its new state. Ready to Fight appearing is said.
 * In single-player modes, a free hand on an arrow says its row's value and
 * way, and A says the new value, or the same at the end of the range.
 * A choice is silent: the game's announcer names the character. Online,
 * other players' choices and slot changes are queued behind; offline they
 * are the players in the room, and silent. A step names where it goes at the
 * press; while the glide runs, hover announcements wait, and arriving where
 * the step said is silent. It compares each snapshot with the previous one
 * and touches no game state itself. */
#pragma once
#include "css_targets.hpp"
#include "game_access.h"
#include <set>
#include <string>

namespace a11y {

class Speech;

/* A glide as the speech follows it, kept by whoever steers. */
struct GlideStatus {
    enum class Phase {
        none,
        /* Under way to destination. */
        gliding,
        /* Ended since the last frame, at the destination or elsewhere. */
        ended,
        /* Ended since the last frame without getting there. */
        failed,
    };
    Phase phase = Phase::none;
    Target destination;
};

class CssSpeech {
public:
    /* speech must outlive this. */
    explicit CssSpeech(Speech& speech) : m_speech(speech) {}

    /* A scene was entered: the next snapshot is an arrival, spoken as the
     * screen opening. */
    void forget() {
        m_seen = false;
        m_pickup_pending = false;
        m_drop_said = false;
    }

    /* The screen's state this frame, and the glide's. Speaks what changed
     * since the last one: the local hand's doings interrupting, other
     * players' queued; nothing when nothing did. While a glide runs, the
     * targets the hand crosses go unsaid, and what the hand does meanwhile
     * waits behind the step's announcement. The frame a glide has ended
     * says "Could not reach" and the destination if it failed, and names
     * what the hand is on unless that is the destination. */
    void frame(const A11yCssState& state, const GlideStatus& glide);

    /* A step: says where it goes, interrupting; to is none when nothing lies
     * that way. A portrait with a coin the hand may pick up says whose coin;
     * a top bar button, that it waits while the hand holds a coin. A step
     * down into the player slots with a coin in the hand first says what the
     * coin going back does, which the drop then leaves unsaid. */
    void step(const A11yCssState& state, Target to);

    /* A target with its value: "Player 2: CPU", "Teams: off". */
    std::string target_words(const A11yCssState& state, Target target);

private:
    std::string opening(const A11yCssState& state);
    /* What the local hand's coin did from last to now, or empty; quiet
     * leaves out the portraits a carried coin enters. */
    std::string coin_announcement(const A11yCssState& last, const A11yCssState& now, bool quiet);
    /* The local hand holds the coin of slot coin, over the portrait now shows
     * under it unless quiet: "Holding your coin. Fox". */
    std::string holding(const A11yCssState& now, int coin, bool quiet);
    /* The local hand let go of the coin of slot coin. */
    std::string dropped(const A11yCssState& last, const A11yCssState& now, int coin);
    /* What the coin of slot coin going back does as the hand carries it down
     * into the player slots: "No character", "Back to Yoshi". */
    std::string drop_words(const A11yCssState& state, int coin);
    /* A single-player mode's difficulty or stock count changed, or A on an
     * arrow, is, at the end of its range: the value. Empty otherwise. */
    std::string arrow_announcement(const A11yCssState& last, const A11yCssState& now, Target is);
    /* A slider the local hand grabbed, moved or let go of, or empty; quiet
     * leaves out its value. */
    std::string slider_announcement(const A11yCssState& last, const A11yCssState& now, bool quiet);
    /* Changes to the player slots and the Teams rule, given what the local
     * hand was and is on: the local hand's appended to mine, other players'
     * to others. */
    void slot_announcements(const A11yCssState& last, const A11yCssState& now, Target was,
        Target is, std::string& mine, std::string& others);
    /* A player slot with its kind and character: "Player 2: CPU, Yoshi". */
    std::string slot_words(const A11yCssState& state, int slot);
    /* A character's name; a missing one is spoken by number and logged. */
    std::string name(A11yCharacter character);

    Speech& m_speech;
    bool m_seen = false;
    /* A coin was picked up last frame and is not announced yet. */
    bool m_pickup_pending = false;
    /* A step said what the carried coin going back does. */
    bool m_drop_said = false;
    A11yCssState m_last{};
    /* The characters missing from the names table that were logged. */
    std::set<A11yCharacter> m_logged_missing;
};

}  // namespace a11y
