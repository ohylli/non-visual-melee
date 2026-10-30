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
 * player's online, and nobody when that port is a CPU or empty.
 *
 * Once the stats panels open, the reader follows the local port's panel:
 * its page changes are said, the game's own A, B and stick. The reading
 * cursor is the fork's own, worked by the local controller's D-pad and Z: a
 * row of that page for one player, said row by row, switched between
 * players with only what differs said, and Z for the player's summary
 * line.
 *
 * Ready is said from the panels' ready flags, compared tick to tick: your
 * Start readying you names every human you wait for, a second Start says
 * you are not ready, and another human's ready and un-ready are queued
 * behind what is being said. A ready that ends the scene says nothing: the
 * next screen speaks. */
#pragma once
#include "game_access.h"
#include "game_text.hpp"
#include "speech.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace a11y {

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

/* One row of a stats panel's page as said: a label, and a value except on
 * a heading. */
struct ResultsRow {
    std::string label;
    std::string value;
    bool has_value = false;

    /* "Damage given, 87"; "KOs" for a heading. */
    std::string text() const;
};

class ResultsSpeech {
public:
    /* speech must outlive this; text decodes the game's text, the results
     * font's own glyphs added. */
    ResultsSpeech(Speech& speech, GameTextSource text) : m_speech(speech), m_text(text) {
        m_text.font_glyphs = results_font_glyphs();
    }

    /* The results scene was entered: a new visit, whose opening is still to
     * come. */
    void visit_started();

    /* The scene after a tick. */
    void tick(const A11yResults& results);

    /* The reading cursor takes the local controller's D-pad, so quick chat
     * leaves it alone on results: while speech is on, as for every feature
     * that takes keys for speech. */
    bool takes_dpad() const { return m_speech.enabled(); }

private:
    void opening(const A11yResults& results);
    /* The panels' first announcement: the page and its first row, with the
     * keys on the first results of the run. */
    void stats_opened(const A11yResults& results, Mode mode);
    /* The reading cursor's moves, one press each. */
    void step(const A11yResults& results, int by);
    void switch_player(const A11yResults& results, int by);
    void summary_line(const A11yResults& results);
    /* Who readied or un-readied since the last tick. */
    void ready_changes(const A11yResults& results);
    /* The page's name and the cursor player's first row. */
    std::string page_announcement(const A11yResults& results);
    /* An opponent as page 0's rows name them: "P2 Mario", their tag in
     * place of the port where the screen shows it, "you" for yours. */
    std::string opponent_name(const A11yResults& results, const A11yResultsOpponent& opponent);
    /* The rows of a player's page, as the screen lists them. */
    std::vector<ResultsRow> rows(const A11yResults& results, int port, int page);
    /* Game text made speakable, its trouble logged once; empty for none. */
    std::string game_words(const std::uint8_t* text, const std::string& what);
    /* The match type's title in the game's words, or empty. */
    std::string match_type(const A11yResults& results);

    Speech& m_speech;
    GameTextSource m_text;
    /* No results of this game run has said its opening yet. */
    bool m_keys_due = true;
    /* This visit's opening has been said. */
    bool m_opened = false;
    /* No results of this game run has opened the stats panels yet. */
    bool m_stats_keys_due = true;
    /* The reading cursor, live once this visit's panels are open: the page
     * of the panel it follows, the player and the row. */
    bool m_stats_open = false;
    int m_page = 0;
    int m_port = 0;
    int m_row = 0;
    /* Each panel's ready flag as last seen, from the panels' opening on. */
    bool m_ready[A11Y_RESULTS_PORTS] = {};
    LogOnce m_log_once;
};

}  // namespace a11y
