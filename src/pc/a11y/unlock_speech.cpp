/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "unlock_speech.hpp"
#include "css_names.hpp"
#include "sentences.hpp"
#include <optional>
#include <string_view>

namespace a11y {
namespace {

/* The fork's own words: the key the screen does not show, and what stands
 * in for a text the reader cannot find, so a notice is never silent. */
constexpr std::string_view kNextKey = "A for next.";
constexpr std::string_view kNoText = "Unlock notice.";

/* The New challenger screen's words as drawn, "WARNING / CHALLENGER
 * APPROACHING" in normal capitalisation; the name stands in for the
 * silhouette after the colon; the key the screen does not show. */
constexpr std::string_view kWarning = "Warning! Challenger approaching!";
constexpr std::string_view kNewFoe = "A new foe has appeared!";
constexpr std::string_view kNewFoeNamed = "A new foe has appeared: ";
constexpr std::string_view kFightKey = "A to fight";

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

void UnlockSpeech::challenger(const A11yChallenger& challenger) {
    std::string line(kWarning);
    if (std::optional<std::string_view> name = character_name(challenger.character)) {
        add_sentence(line, std::string(kNewFoeNamed) + std::string(*name));
    } else {
        m_log_once(
            "[a11y] new challenger: no name for character " + std::to_string(challenger.kind));
        add_sentence(line, kNewFoe);
    }
    std::string key;
    if (challenger.humans >= 2) {
        /* The player label, as the match announcements make it. */
        std::string label = spoken_text(decode_shift_jis(challenger.tag).text);
        add_part(key, label.empty() ? "P" + std::to_string(challenger.port + 1) : label);
    }
    add_part(key, kFightKey);
    add_sentence(line, key);
    end_sentence(line);
    m_speech.announce(line, Mode::interrupt);
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
