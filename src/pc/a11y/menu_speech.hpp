/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Menu speech: reads the main menu tree aloud from a snapshot of its state
 * taken once a frame (docs/adr/0003-main-menu-tree-is-polled.md). A tree
 * screen opening says its title, hovered entry and description; the cursor
 * moving says the entry and description. Sound, Screen display, Multi-Man
 * Melee and the four rules screens have a reader: opening says the title,
 * row and value, key hint and description; a row or choice changing says it
 * with its description; a value changing says the value alone, then the
 * description where the description depends on the value. Item Switch and
 * Random Stage draw no description; a row with a hint of its own (the item
 * frequency) says it the first time it is reached after the screen opens,
 * and a refused change (Random Stage's last stage) says why. Coming back
 * from a screen a row opens is an opening without the hint. Any other leaf
 * screen opening says its name and "No speech yet."
 * (.scratch/main-menu-tree/spec.md, .scratch/rules-screens/spec.md). It
 * compares each snapshot with the previous one and touches no game state
 * itself. */
#pragma once
#include "game_access.h"
#include "game_text.hpp"
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace a11y {

class Speech;

class MenuSpeech {
public:
    /* speech must outlive this; text decodes the descriptions' game text. */
    MenuSpeech(Speech& speech, GameTextSource text) : m_speech(speech), m_text(text) {}

    /* A scene was entered: the next snapshot is an arrival, spoken as the
     * screen opening even when it matches the last one seen. */
    void forget() { m_seen = false; }

    /* The tree's state this frame. Speaks what changed since the last one,
     * interrupting; nothing when nothing did. */
    void frame(const A11yMenuState& state);

private:
    /* returned: back from a screen one of its rows opens. */
    std::string opening(const A11yMenuState& state, bool returned);
    std::string hovered_entry(const A11yMenuState& state);
    /* The row in view as it is reached: row(), with the row's own hint the
     * first time since the screen opened. */
    std::string row_reached(const A11yMenuState& state);
    /* On a leaf screen with a reader, the row or choice in view, with the
     * row's value: "Channel: Stereo", "10-Man Melee", "Time limit: none". */
    std::string row(const A11yMenuState& state);
    /* The value on that row, in the screen's words; nothing where the row has
     * none, as for a Multi-Man Melee choice, or is unknown. */
    std::optional<std::string> row_value(const A11yMenuState& state);
    /* A value's word, or its number where the names table lacks it. */
    std::string known(std::optional<std::string_view> word, int number, const A11yMenuState& state);
    /* The description of the row in view, as one spoken line. */
    std::string row_description(const A11yMenuState& state);
    /* A description as one spoken line, from the base port's plain text or
     * from game text; where names it in log lines. */
    std::string description(
        const char* plain, const std::uint8_t* game_text, const std::string& where);
    /* A log line for a gap in the words, once per text. */
    void log_once(const std::string& line);

    Speech& m_speech;
    GameTextSource m_text;
    bool m_seen = false;
    int m_menu = 0;
    int m_hovered = 0;
    /* The row in view on a reader's screen, by its key (RowKey). */
    int m_row = -1;
    std::optional<std::string> m_value;
    /* The rows' own hints said since the screen opened. */
    std::set<std::string_view> m_row_hints;
    std::set<std::string> m_logged;
};

}  // namespace a11y
