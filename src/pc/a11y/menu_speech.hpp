/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Menu speech: reads the main menu tree aloud from a snapshot of its state
 * taken once a frame (docs/adr/0003-main-menu-tree-is-polled.md). A tree
 * screen opening says its title, hovered entry and description; the cursor
 * moving says the entry and description; a leaf screen opening says its name
 * and "No speech yet." (.scratch/main-menu-tree/spec.md). It compares each
 * snapshot with the previous one and touches no game state itself. */
#pragma once
#include "game_access.h"
#include "game_text.hpp"
#include <set>
#include <string>

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
    std::string opening(const A11yMenuState& state);
    std::string hovered_entry(const A11yMenuState& state);
    std::string description(const A11yMenuState& state);
    /* A log line for a gap in the words, once per text. */
    void log_once(const std::string& line);

    Speech& m_speech;
    GameTextSource m_text;
    bool m_seen = false;
    int m_menu = 0;
    int m_hovered = 0;
    std::set<std::string> m_logged;
};

}  // namespace a11y
