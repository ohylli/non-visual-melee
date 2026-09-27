/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_speech.hpp"
#include "menu_names.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <optional>
#include <string_view>

namespace a11y {
namespace {

bool log_enabled() {
    static const bool enabled = config_from_environment().log;
    return enabled;
}

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
    std::string out;
    for (std::size_t i = 0; i < text.size(); i++) {
        if (text.substr(i, 7) == "{glyph ") {
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

/* Decoded text for a log line, its breaks shown as " / " and its unknown
 * glyphs marked. */
std::string one_line(std::string text) {
    for (std::size_t at = text.find('\n'); at != std::string::npos; at = text.find('\n', at)) {
        text.replace(at, 1, " / ");
    }
    return text;
}

/* A leaf screen's centre text, as log lines name it. */
std::string where_text(const A11yMenuState& state) {
    return "[a11y] menu " + std::to_string(state.menu) + " centre text " +
           std::to_string(state.center_text);
}

/* The key hint of a leaf screen with a reader, spoken once as it opens;
 * nothing for any other screen. */
std::optional<std::string_view> reader_hint(int menu) {
    switch (menu) {
    case kMenuSound:
        return "Left and right to change.";
    case kMenuDisplay:
        return "A to change.";
    case kMenuMultiMan:
        return "Left and right to choose.";
    default:
        return std::nullopt;
    }
}

/* The value on a reader's row, in the screen's words; nothing where the row
 * has none, as for a Multi-Man Melee choice. */
std::optional<std::string> row_value(const A11yMenuState& state) {
    if (state.menu == kMenuSound && state.center_text == kTextSoundChannel) {
        return std::string(channel_word(state.mono));
    }
    if (state.menu == kMenuSound && state.center_text == kTextSoundVolume) {
        return balance_words(state.balance);
    }
    if (state.menu == kMenuDisplay) {
        return std::string(on_off_word(state.deflicker));
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
    bool reader = reader_hint(state.menu).has_value();
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
        add_sentence(out, description(nullptr, state.center_text_string, where_text(state)));
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
    } else if (std::optional<std::string_view> hint = reader_hint(state.menu)) {
        add_sentence(out, row(state));
        add_sentence(out, *hint);
        add_sentence(out, description(nullptr, state.center_text_string, where_text(state)));
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
        log_once("[a11y] menu " + std::to_string(state.menu) + " entry " +
                 std::to_string(state.hovered) + ": not in the menu names table");
        add_sentence(out, "Unknown entry " + std::to_string(state.hovered));
    }
    add_sentence(out, description(state.pc_description, state.description,
                          "[a11y] menu " + std::to_string(state.menu) + " entry " +
                              std::to_string(state.hovered)));
    return out;
}

std::string MenuSpeech::row(const A11yMenuState& state) {
    std::string out;
    if (std::optional<std::string_view> name = center_text_name(state.menu, state.center_text)) {
        out = *name;
    } else {
        log_once(where_text(state) + ": not in the menu names table");
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
