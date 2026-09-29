/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_speech.hpp"
#include "menu_names.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <optional>
#include <string_view>

namespace a11y {
namespace {

/* Parts of one announcement as sentences: "Main Menu. 1-P Mode. Solo
 * Smash!" A part that ends a sentence itself keeps its own mark. */
void add_sentence(std::string& out, std::string_view part) {
    if (part.empty()) {
        return;
    }
    if (!out.empty()) {
        char last = out.back();
        if (last != '.' && last != '!' && last != '?') {
            out += '.';
        }
        out += ' ';
    }
    out += part;
}

/* A decoded description as one spoken line: the lines of a wrapped
 * description joined by a space, the marks of unknown glyphs
 * ("{glyph 4000}") left out. */
std::string spoken_text(std::string_view text) {
    constexpr std::string_view kGlyphMark = "{glyph ";
    std::string out;
    for (std::size_t i = 0; i < text.size(); i++) {
        if (text.substr(i).starts_with(kGlyphMark)) {
            std::size_t close = text.find('}', i);
            if (close != std::string_view::npos) {
                i = close;
                continue;
            }
        }
        char c = text[i] == '\n' ? ' ' : text[i];
        if (c == ' ' && (out.empty() || out.back() == ' ')) {
            continue;
        }
        out += c;
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

/* A tree screen's hovered entry, as log lines name it. */
std::string entry_where(const A11yMenuState& state) {
    return "[a11y] menu " + std::to_string(state.menu) + " entry " + std::to_string(state.hovered);
}

/* A leaf screen's centre text, as log lines name it. */
std::string center_text_where(const A11yMenuState& state) {
    return "[a11y] menu " + std::to_string(state.menu) + " centre text " +
           std::to_string(state.center_text);
}

/* The reader's row the centre text names; nothing on a screen without a
 * reader or for a centre text its reader lacks. */
const ReaderRow* reader_row(const A11yMenuState& state) {
    const Reader* reader = menu_reader(state.menu);
    return reader != nullptr ? reader->row(state.center_text) : nullptr;
}

/* The value on a reader's row, in the screen's words; nothing where the row
 * has none, as for a Multi-Man Melee choice, or is unknown. */
std::optional<std::string> row_value(const A11yMenuState& state) {
    const ReaderRow* row = reader_row(state);
    if (row == nullptr) {
        return std::nullopt;
    }
    switch (row->value) {
    case RowValue::channel:
        return std::string(channel_word(state.mono));
    case RowValue::balance:
        return balance_words(state.balance);
    case RowValue::deflicker:
        return std::string(on_off_word(state.deflicker));
    case RowValue::none:
        break;
    }
    return std::nullopt;
}

/* A value spoken on its own starts a sentence: "Centre". */
std::string capitalised(std::string text) {
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z') {
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    }
    return text;
}

}  // namespace

void MenuSpeech::frame(const A11yMenuState& state) {
    bool arrived = !m_seen || state.menu != m_menu;
    bool reader = menu_reader(state.menu) != nullptr;
    bool moved = !arrived && state.hovered != m_hovered && is_tree_screen(state.menu);
    bool row_changed = !arrived && reader && state.center_text != m_center_text;
    std::optional<std::string> value = row_value(state);
    bool value_changed = !arrived && reader && !row_changed && value && value != m_value;
    m_seen = true;
    m_menu = state.menu;
    m_hovered = state.hovered;
    m_center_text = state.center_text;
    m_value = value;
    if (arrived) {
        m_speech.announce(opening(state), Mode::interrupt);
    } else if (moved) {
        m_speech.announce(hovered_entry(state), Mode::interrupt);
    } else if (row_changed) {
        std::string out = row(state);
        add_sentence(out, description(nullptr, state.center_text_string, center_text_where(state)));
        m_speech.announce(out, Mode::interrupt);
    } else if (value_changed) {
        m_speech.announce(capitalised(*value), Mode::interrupt);
    }
}

std::string MenuSpeech::opening(const A11yMenuState& state) {
    std::optional<std::string_view> name = menu_screen_name(state.menu);
    std::string out;
    if (!name) {
        log_once("[a11y] menu " + std::to_string(state.menu) + ": not in the menu names table");
        return "Unknown screen " + std::to_string(state.menu);
    }
    add_sentence(out, *name);
    if (is_tree_screen(state.menu)) {
        add_sentence(out, hovered_entry(state));
    } else if (const Reader* reader = menu_reader(state.menu)) {
        add_sentence(out, row(state));
        add_sentence(out, reader->hint);
        add_sentence(out, description(nullptr, state.center_text_string, center_text_where(state)));
    } else {
        add_sentence(out, "No speech yet.");
    }
    return out;
}

std::string MenuSpeech::hovered_entry(const A11yMenuState& state) {
    std::string out;
    if (state.pc_label != nullptr) {
        add_sentence(out, plain_capitals(state.pc_label));
    } else if (std::optional<std::string_view> name = menu_entry_name(state.menu, state.hovered)) {
        add_sentence(out, *name);
    } else {
        log_once(entry_where(state) + ": not in the menu names table");
        add_sentence(out, "Unknown entry " + std::to_string(state.hovered));
    }
    add_sentence(out, description(state.pc_description, state.description, entry_where(state)));
    return out;
}

std::string MenuSpeech::row(const A11yMenuState& state) {
    std::string out;
    if (const ReaderRow* named = reader_row(state)) {
        out = named->name;
    } else {
        log_once(center_text_where(state) + ": not in the menu names table");
        out = "Unknown row " + std::to_string(state.center_text);
    }
    if (std::optional<std::string> value = row_value(state)) {
        out += ": " + *value;
    }
    return out;
}

std::string MenuSpeech::description(
    const char* plain, const std::uint8_t* game_text, const std::string& where) {
    if (plain != nullptr) {
        return spoken_text(plain);
    }
    if (game_text == nullptr) {
        log_once(where + ": no description");
        return "";
    }
    DecodedText decoded = decode_game_text(game_text, m_text);
    if (decoded.unknown_glyphs > 0 || decoded.malformed) {
        log_once(where + ": description \"" + one_line(decoded.text) + "\"" +
                 (decoded.unknown_glyphs > 0 ? ", unknown glyphs left out" : "") +
                 (decoded.malformed ? ", malformed" : ""));
    }
    return spoken_text(decoded.text);
}

void MenuSpeech::log_once(const std::string& line) {
    if (m_logged.insert(line).second && log_enabled()) {
        pc_log_line("%s", line.c_str());
    }
}

}  // namespace a11y
