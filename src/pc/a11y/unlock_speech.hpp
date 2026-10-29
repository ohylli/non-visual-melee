/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Unlock notice speech: reads the unlock notice scene's notices aloud in the
 * game's own words (.scratch/unlock-notices/spec.md), from one hook: the
 * notice the scene sets up, by the number of its text in the prize table
 * (SdPrize), a trophy's composed into the table's placeholder first.
 *
 * Each notice says its text, interrupting the one before; the first of a
 * visit adds the key. The date, the category icon and a count are left
 * out, and the scene itself stays silent: its first notice is its opening.
 *
 * The New challenger screen, which leads to an unlock, says its words, the
 * challenger its silhouette hides, and the key, with the challenged
 * player's label before it when the match had two or more humans. The
 * reader touches no game state. */
#pragma once
#include "game_access.h"
#include "game_text.hpp"
#include "speech.hpp"
#include <cstdint>
#include <string>

namespace a11y {

/* The prize table's strings (SdPrize) by the number the game's code uses;
 * nullptr for one it cannot find. */
using PrizeStrings = const std::uint8_t* (*)(int number);

class UnlockSpeech {
public:
    /* speech must outlive this; text decodes the game's text, the prize
     * font's own glyphs added, strings finds it. */
    UnlockSpeech(Speech& speech, GameTextSource text, PrizeStrings strings);

    /* The unlock notice scene was entered: its first notice says the key. */
    void visit_started();

    /* The scene sets up a notice whose text is string text_id, the first or
     * the next after A or Start. */
    void notice(int text_id);

    /* The New challenger screen was entered, with this challenger. A kind
     * with no name is left out of the line and logged once. */
    void challenger(const A11yChallenger& challenger);

private:
    /* String number of the game's text made speakable, its trouble logged
     * once; empty for none. */
    std::string game_words(int number);

    Speech& m_speech;
    GameTextSource m_text;
    PrizeStrings m_strings;
    /* No notice of this visit has been said yet. */
    bool m_key_due = true;
    LogOnce m_log_once;
};

}  // namespace a11y
