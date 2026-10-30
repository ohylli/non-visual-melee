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

/* A trophy's notice (unlock number 0x3E) is composed at run time into the
 * prize table's placeholder string 0x4A (un_802FE918 in ifprize.c), which
 * holds the whole message once the hook runs; the string the frame binds
 * is then only its first part ("You got the "). */
constexpr int kTrophyUnlock = 0x3E;
constexpr int kTrophyText = 0x4A;

/* The New challenger screen's words as drawn, "WARNING / CHALLENGER
 * APPROACHING" in normal capitalisation; the name stands in for the
 * silhouette after the colon; the key the screen does not show. */
constexpr std::string_view kWarning = "Warning! Challenger approaching!";
constexpr std::string_view kNewFoe = "A new foe has appeared!";
constexpr std::string_view kNewFoeNamed = "A new foe has appeared: ";
constexpr std::string_view kFightKey = "A to fight";
/* After the challenged player's label when two or more humans played. */
constexpr std::string_view kFights = " fights";

}  // namespace

UnlockSpeech::UnlockSpeech(Speech& speech, GameTextSource text, PrizeStrings strings)
    : m_speech(speech), m_text(text), m_strings(strings) {
    m_text.font_glyphs = prize_font_glyphs();
}

void UnlockSpeech::visit_started() {
    m_key_due = true;
}

void UnlockSpeech::notice(int unlock, int text_id) {
    std::string text = game_words(unlock == kTrophyUnlock ? kTrophyText : text_id);
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
    if (challenger.humans >= 2) {
        add_sentence(line,
            player_label(challenger.tag, challenger.port, m_log_once, "[a11y] new challenger:") +
                std::string(kFights));
    }
    add_sentence(line, kFightKey);
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
