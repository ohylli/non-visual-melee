/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Training menu speech: reads the Training menu aloud from a snapshot taken
 * once a tick in the Training scene (.scratch/training-menu/spec.md). The
 * opening says the menu and the row the cursor is on with its value, and on
 * the first opening of each Training session the keys; then each row the
 * cursor reaches with its value, and each new value of the row it is on.
 * Closing says nothing, nor do the actions (A on Item, Reset, Finish), and a
 * value changed on another row than the cursor's, as Reset changes them, is
 * left unsaid. It compares each snapshot with the previous one and touches no
 * game state itself.
 *
 * Changes that come close together, as a held direction's auto-repeat makes
 * them, are thinned so the speech never falls behind: a change at least a
 * quarter second after the one before is said at once; a sooner one is held
 * back, and the newest held is said once a quarter second passes with no
 * other. The quarter second is real time, so slow motion, which slows the
 * menu and its repeats with the match, does not stretch it. */
#pragma once
#include "game_access.h"
#include <chrono>
#include <optional>
#include <set>
#include <string>

namespace a11y {

class Speech;

using Clock = std::chrono::steady_clock;

class TrainingSpeech {
public:
    /* How long after a change the next is said at once rather than held, and
     * how long a held change waits for quiet; to tune by ear. */
    static constexpr Clock::duration kSettle = std::chrono::milliseconds(250);

    /* speech must outlive this. */
    explicit TrainingSpeech(Speech& speech) : m_speech(speech) {}

    /* The Training scene was entered: a new session, whose first opening
     * says the keys. Forgets the last session's menu. */
    void session_started();

    /* The menu's state after a tick, and the time it was read. Speaks what
     * changed since the last tick: the opening interrupting, each change at
     * once or held back as above. */
    void tick(const A11yTrainingMenu& menu, Clock::time_point now);

private:
    /* A row with its value ("Item: Food", "Reset"); nothing for the hidden
     * row or an unknown one. */
    std::optional<std::string> row_text(int row, int value);
    /* A value's words, or its number when the table lacks it. */
    std::string value_text(int row, int value);
    void opening(const A11yTrainingMenu& menu, Clock::time_point now);
    void log_once(const std::string& line);

    Speech& m_speech;
    bool m_hint_due = true;
    bool m_open = false;
    int m_row = -1;
    int m_value = 0;
    /* When the opening or the last change came. */
    Clock::time_point m_last_change{};
    /* The newest change held back, waiting for quiet. */
    std::optional<std::string> m_held;
    /* Nothing has been said since the opening: a change held back from it
     * is said after the opening instead of cutting it off. */
    bool m_after_opening = false;
    std::set<std::string> m_logged;
};

}  // namespace a11y
