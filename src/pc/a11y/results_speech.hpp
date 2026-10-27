/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Results speech: reads the results scene aloud from a snapshot taken once a
 * tick (.scratch/results/spec.md). Nothing is said on entry, while the
 * announcer names the winner; the opening comes once the summary table is
 * up and presses start to count (state 2): the match type, the winner, each
 * player in place order with their summary, and "Start when ready.", with
 * the keys on the first results of the game run. It compares each snapshot
 * with the previous one and touches no game state itself.
 *
 * "You" are the local player: controller 1's port offline, the local
 * player's online, and nobody when that port is a CPU or empty. */
#pragma once
#include "game_access.h"
#include "game_text.hpp"
#include <set>
#include <string>
#include <vector>

namespace a11y {

class Speech;

/* Your port, or -1 when nobody is "you". */
int results_you(const A11yResults& results);

/* The ports with a player, in the order the opening says them: by place,
 * the player the winner's banner names first among the tied, then by the
 * game's own order and by port; by port on a cancelled match. */
std::vector<int> results_order(const A11yResults& results);

/* A player as the summary names them: "Mario, P2", "Link, P1, you". */
std::string results_player_name(const A11yResults& results, int port);

/* A player's summary column, the rows the match type shows: "2 KOs, 0 falls,
 * 0 SDs, not KO'd"; "19400 points" in Bonus mode. */
std::string results_summary(const A11yResults& results, int port);

class ResultsSpeech {
public:
    /* speech must outlive this; text decodes the game's text. */
    ResultsSpeech(Speech& speech, GameTextSource text) : m_speech(speech), m_text(text) {}

    /* The results scene was entered: a new visit, whose opening is still to
     * come. */
    void visit_started();

    /* The scene after a tick. */
    void tick(const A11yResults& results);

private:
    void opening(const A11yResults& results);
    /* The match type's title in the game's words, or empty. */
    std::string match_type(const A11yResults& results);
    void log_once(const std::string& line);

    Speech& m_speech;
    GameTextSource m_text;
    /* No results of this game run has said its opening yet. */
    bool m_keys_due = true;
    /* This visit's opening has been said. */
    bool m_opened = false;
    std::set<std::string> m_logged;
};

}  // namespace a11y
