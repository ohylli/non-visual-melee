/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "training_speech.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include "training_names.hpp"

namespace a11y {

void TrainingSpeech::session_started() {
    m_hint_due = true;
    m_open = false;
    m_row = -1;
    m_value = 0;
    m_spoken_row = -1;
    m_held.reset();
    m_after_opening = false;
}

void TrainingSpeech::tick(const A11yTrainingMenu& menu, Clock::time_point now) {
    if (!menu.open) {
        /* Closing says nothing and drops what was held. */
        m_open = false;
        m_held.reset();
        return;
    }
    if (!m_open) {
        opening(menu, now);
        return;
    }

    /* A value change on a row the player has not yet heard named, its row
     * move held back and now replaced, says the row too, or the value would
     * sound like one of the last row named. */
    std::optional<std::string> change;
    if (menu.row != m_row) {
        change = row_text(menu.row, menu.value);
    } else if (menu.value != m_value) {
        const TrainingRow* row = training_row(menu.row);
        if (row != nullptr && row->value != TrainingValue::none) {
            change = menu.row == m_spoken_row ? value_text(menu.row, menu.value) :
                                                row_text(menu.row, menu.value);
        }
    }
    m_row = menu.row;
    m_value = menu.value;

    if (change) {
        if (now - m_last_change >= kSettle) {
            m_held.reset();
            m_after_opening = false;
            m_spoken_row = menu.row;
            m_speech.announce(*change, Mode::interrupt);
        } else {
            m_held = std::move(change);
        }
        m_last_change = now;
    } else if (m_held && now - m_last_change >= kSettle) {
        /* A change held back from the opening comes after it: the press
         * landed while the panel was still sliding in. */
        Mode mode = m_after_opening ? Mode::queue : Mode::interrupt;
        m_after_opening = false;
        m_spoken_row = m_row;
        m_speech.announce(*m_held, mode);
        m_held.reset();
    }
}

void TrainingSpeech::opening(const A11yTrainingMenu& menu, Clock::time_point now) {
    m_open = true;
    m_row = menu.row;
    m_value = menu.value;
    m_spoken_row = menu.row;
    m_held.reset();
    m_last_change = now;
    m_after_opening = true;

    std::string text(kTrainingMenu);
    text += '.';
    if (std::optional<std::string> row = row_text(menu.row, menu.value)) {
        text += ' ';
        text += *row;
        text += '.';
    }
    if (m_hint_due) {
        text += ' ';
        text += kTrainingHint;
        m_hint_due = false;
    }
    m_speech.announce(text, Mode::interrupt);
}

std::optional<std::string> TrainingSpeech::row_text(int row, int value) {
    const TrainingRow* named = training_row(row);
    if (named == nullptr) {
        log_once("[a11y] training menu row " + std::to_string(row) +
                 ": not in the training menu names table");
        return std::nullopt;
    }
    std::string text(named->name);
    if (named->value != TrainingValue::none) {
        text += named->separator;
        text += value_text(row, value);
    }
    return text;
}

std::string TrainingSpeech::value_text(int row, int value) {
    const TrainingRow* named = training_row(row);
    if (named != nullptr) {
        if (std::optional<std::string> words = training_value_words(named->value, value)) {
            return *words;
        }
    }
    log_once("[a11y] training menu row " + std::to_string(row) + ": value " +
             std::to_string(value) + " not in the training menu names table");
    return std::to_string(value);
}

void TrainingSpeech::log_once(const std::string& line) {
    if (m_logged.insert(line).second && log_enabled()) {
        pc_log_line("%s", line.c_str());
    }
}

}  // namespace a11y
