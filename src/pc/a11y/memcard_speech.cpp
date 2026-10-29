/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "memcard_speech.hpp"
#include "sentences.hpp"
#include <string_view>

namespace a11y {
namespace {

/* The fork's own words: the keys the screen does not show. */
constexpr std::string_view kChoiceKeys = "Left and Right choose, A confirms.";
constexpr std::string_view kContinueKey = "A to continue.";

/* The choices' strings in SdMsgBox, by kind and side (gm_801AEE6C), with
 * the fork's word for each should the table lack it. */
struct Choice {
    int number;
    std::string_view fallback;
};
constexpr Choice kYesNo[2] = {{3, "Yes"}, {4, "No"}};
constexpr Choice kOkRetry[2] = {{2, "OK"}, {5, "Try Again"}};

/* The choices of a kind of prompt, or nullptr for a message. */
const Choice* choices_of(MemcardKind kind) {
    switch (kind) {
    case MemcardKind::yes_no:
        return kYesNo;
    case MemcardKind::ok_retry:
        return kOkRetry;
    case MemcardKind::message:
        break;
    }
    return nullptr;
}

}  // namespace

void MemcardSpeech::visit_started() {
    m_shown = false;
}

void MemcardSpeech::prompt(int kind, int text_id) {
    m_shown = true;
    m_kind = static_cast<MemcardKind>(kind);
    m_text_id = text_id;
    m_frames = 0;
    m_said = false;
}

void MemcardSpeech::frame(int hovered_choice) {
    if (!m_shown) {
        return;
    }
    int choice = hovered_choice == 0 ? 0 : 1;
    if (!m_said) {
        /* The frame after the one that showed it may still replace it, as
         * "Creating new Game Data." is: said once that frame is over. */
        if (++m_frames < 2) {
            return;
        }
        say_prompt(choice);
        return;
    }
    if (choices_of(m_kind) != nullptr && choice != m_choice) {
        m_choice = choice;
        m_speech.announce(choice_name(choice), Mode::interrupt);
    }
}

void MemcardSpeech::say_prompt(int choice) {
    m_said = true;
    m_choice = choice;
    std::string text = game_words(m_text_id);
    if (choices_of(m_kind) != nullptr) {
        add_sentence(text, choice_name(choice));
        if (m_keys_due) {
            m_keys_due = false;
            add_sentence(text, kChoiceKeys);
        }
    } else {
        if (m_kind != MemcardKind::message) {
            m_log_once("[a11y] memory card: prompt kind " +
                       std::to_string(static_cast<int>(m_kind)) + " said as a message");
        }
        add_sentence(text, kContinueKey);
    }
    end_sentence(text);
    /* The first waits for the startup's own lines. */
    m_speech.announce(text, m_first ? Mode::queue : Mode::interrupt);
    m_first = false;
}

std::string MemcardSpeech::choice_name(int choice) {
    const Choice& entry = choices_of(m_kind)[choice];
    std::string words = game_words(entry.number);
    return words.empty() ? std::string(entry.fallback) : words;
}

std::string MemcardSpeech::game_words(int number) {
    const std::uint8_t* bytes = m_strings != nullptr ? m_strings(number) : nullptr;
    if (bytes == nullptr) {
        m_log_once("[a11y] memory card: no game text for text " + std::to_string(number));
        return "";
    }
    DecodedText decoded = decode_game_text(bytes, m_text);
    if (std::string trouble = decode_trouble(decoded); !trouble.empty()) {
        m_log_once("[a11y] memory card: text " + std::to_string(number) + " " + trouble);
    }
    return spoken_text(decoded.text);
}

}  // namespace a11y
