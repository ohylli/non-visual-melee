/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "unlock_speech.hpp"
#include "sentences.hpp"
#include <string_view>

namespace a11y {
namespace {

/* The fork's own words: the key the screen does not show, and what stands
 * in for a text the reader cannot find, so a notice is never silent. */
constexpr std::string_view kNextKey = "A for next.";
constexpr std::string_view kNoText = "Unlock notice.";

}  // namespace

UnlockSpeech::UnlockSpeech(Speech& speech, GameTextSource text, PrizeStrings strings)
    : m_speech(speech), m_text(text), m_strings(strings) {
    m_text.font_glyphs = prize_font_glyphs();
}

void UnlockSpeech::visit_started() {
    m_key_due = true;
}

void UnlockSpeech::notice(int text_id) {
    std::string text = game_words(text_id);
    if (text.empty()) {
        text = kNoText;
    }
    if (m_key_due) {
        m_key_due = false;
        add_sentence(text, kNextKey);
    }
    m_speech.announce(text, Mode::interrupt);
}

std::string UnlockSpeech::game_words(int number) {
    const std::uint8_t* bytes = m_strings != nullptr ? m_strings(number) : nullptr;
    if (bytes == nullptr) {
        m_log_once("[a11y] unlock notice: no game text for text " + std::to_string(number));
        return "";
    }
    return spoken_game_text(
        bytes, m_text, m_log_once, "[a11y] unlock notice: text " + std::to_string(number));
}

}  // namespace a11y
