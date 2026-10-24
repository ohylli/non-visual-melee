/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "direct_speech.hpp"
#include "speech.hpp"
#include <algorithm>
#include <string_view>

namespace a11y {
namespace {

/* Wording is a first proposal for the play test to judge. */
constexpr std::string_view kBackTo = "Back to ";
constexpr std::string_view kConnectionLost = "Connection lost";

/* The line with runs of spaces, the screen's padding and spacing, made one,
 * and none at either end. */
std::string collapsed(std::string_view line) {
    std::string out;
    for (char c : line) {
        if (c == ' ' && (out.empty() || out.back() == ' ')) {
            continue;
        }
        out += c;
    }
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

/* The parts as one announcement, each ended with a full stop unless it ends
 * with its own; the last left as it is. */
std::string joined(std::initializer_list<std::string_view> parts) {
    std::string out;
    for (std::string_view part : parts) {
        if (part.empty()) {
            continue;
        }
        if (!out.empty()) {
            char last = out.back();
            out += last == '.' || last == '!' || last == '?' ? " " : ". ";
        }
        out += part;
    }
    return out;
}

/* A hub row without a recent opponent's "played 5m ago", which the clock
 * changes while the row stays the same. */
std::string_view row_identity(std::string_view row) {
    size_t played = row.rfind(" played ");
    return played == std::string_view::npos ? row : row.substr(0, played);
}

/* The keys' layout in words: each run of consecutive characters by its ends,
 * then the row width: "A to Z, then 2 to 7, 8 to a row, edges wrap". */
std::string layout_words(std::string_view keys) {
    std::string out;
    for (size_t start = 0; start < keys.size();) {
        size_t end = start;
        while (end + 1 < keys.size() && keys[end + 1] == keys[end] + 1) {
            end++;
        }
        if (!out.empty()) {
            out += ", then ";
        }
        out += keys[start];
        if (end > start) {
            out += " to ";
            out += keys[end];
        }
        start = end + 1;
    }
    out += ", " + std::to_string(A11Y_DIRECT_KEY_COLS) + " to a row, edges wrap";
    return out;
}

}  // namespace

DirectSpeech::Shown DirectSpeech::shown(const A11yLobbyState& state) {
    const A11yDirectState& direct = state.direct;
    Shown out;
    out.screen = direct.screen;
    out.title = collapsed(direct.title);
    out.subtitle = collapsed(direct.subtitle);
    out.message = collapsed(state.message);
    out.hint = collapsed(direct.hint);
    int rows = std::clamp(direct.row_count, 0, static_cast<int>(A11Y_DIRECT_ROWS));
    for (int i = 0; i < rows; i++) {
        out.rows.push_back(collapsed(direct.rows[i]));
    }
    out.cursor = direct.cursor;
    if (direct.screen == A11Y_DIRECT_KEYS) {
        std::string_view keys(direct.keys);
        if (direct.key_cursor >= 0 && static_cast<size_t>(direct.key_cursor) < keys.size()) {
            out.hovered_key = keys[direct.key_cursor];
        }
        out.layout = layout_words(keys);
        /* The entry line, "Friend's code   # K3X-Q2_-__": a slot is a
         * character typed or a '_' still to type. */
        std::string_view entry(direct.rows[0]);
        size_t hash = entry.rfind('#');
        if (rows > 0 && hash != std::string_view::npos) {
            for (char c : entry.substr(hash + 1)) {
                if (c == '_') {
                    out.code_length++;
                } else if (c != '-' && c != ' ') {
                    out.typed += c;
                    out.code_length++;
                }
            }
        }
    }
    return out;
}

void DirectSpeech::frame(const A11yLobbyState& state) {
    if (!state.direct_connect) {
        return;
    }
    Shown now = shown(state);
    if (!m_seen) {
        opening(now);
        m_seen = true;
    } else if (now.screen != m_last.screen) {
        screen_changed(now);
    } else if (now.screen == A11Y_DIRECT_HUB) {
        hub_changes(now);
    } else if (now.screen == A11Y_DIRECT_KEYS) {
        keys_changes(now);
    }
    m_last = std::move(now);
}

namespace {

std::string_view hovered_row(const std::vector<std::string>& rows, int cursor) {
    if (cursor < 0 || static_cast<size_t>(cursor) >= rows.size()) {
        return {};
    }
    return rows[cursor];
}

}  // namespace

void DirectSpeech::opening(const Shown& now) {
    switch (now.screen) {
    case A11Y_DIRECT_HUB: {
        std::string_view row = hovered_row(now.rows, now.cursor);
        std::string text;
        switch (m_arrival) {
        case LobbyArrival::from_menu:
            text = joined({now.title, now.subtitle, row, now.message, now.hint});
            break;
        case LobbyArrival::backed_out:
            text = joined({std::string(kBackTo) + now.title, row});
            break;
        case LobbyArrival::connection_lost:
            text = joined({kConnectionLost, now.title, row});
            break;
        }
        m_speech.announce(text, Mode::interrupt);
        break;
    }
    case A11Y_DIRECT_KEYS:
        m_speech.announce(keys_opening(now), Mode::interrupt);
        break;
    case A11Y_DIRECT_CALL:
        break;
    }
}

void DirectSpeech::screen_changed(const Shown& now) {
    switch (now.screen) {
    case A11Y_DIRECT_HUB:
        /* Back from the keyboard or a failed call, still in the lobby. */
        m_speech.announce(joined({now.title, hovered_row(now.rows, now.cursor)}), Mode::interrupt);
        break;
    case A11Y_DIRECT_KEYS:
        m_speech.announce(keys_opening(now), Mode::interrupt);
        break;
    case A11Y_DIRECT_CALL:
        break;
    }
}

std::string DirectSpeech::keys_opening(const Shown& now) const {
    return joined({now.message, now.layout, now.hint, now.hovered_key});
}

void DirectSpeech::hub_changes(const Shown& now) {
    std::string_view row = hovered_row(now.rows, now.cursor);
    /* A press moves the cursor; a code coming onto or leaving the clipboard
     * changes the rows, and the game puts the cursor on the top one. */
    bool moved = now.cursor != m_last.cursor || now.rows.size() != m_last.rows.size() ||
                 row_identity(row) != row_identity(hovered_row(m_last.rows, m_last.cursor));
    if (moved) {
        m_speech.announce(row, Mode::interrupt);
    }
    if (now.message != m_last.message) {
        m_speech.announce(now.message, moved ? Mode::queue : Mode::interrupt);
    }
}

void DirectSpeech::keys_changes(const Shown& now) {
    if (now.typed.size() > m_last.typed.size()) {
        /* Typing clears the error, which the character's own words cover. */
        m_speech.announce(std::string(1, now.typed.back()) + ", " +
                              std::to_string(now.typed.size()) + " of " +
                              std::to_string(now.code_length),
            Mode::interrupt);
        return;
    }
    if (now.typed.size() < m_last.typed.size()) {
        m_speech.announce("Deleted " + m_last.typed.substr(now.typed.size(), 1), Mode::interrupt);
        return;
    }
    bool moved = now.hovered_key != m_last.hovered_key;
    if (moved) {
        m_speech.announce(now.hovered_key, Mode::interrupt);
    }
    if (now.message != m_last.message) {
        m_speech.announce(now.message, moved ? Mode::queue : Mode::interrupt);
    }
}

}  // namespace a11y
