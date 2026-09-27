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

}  // namespace

void MenuSpeech::frame(const A11yMenuState& state) {
    bool arrived = !m_seen || state.menu != m_menu;
    bool moved = !arrived && state.hovered != m_hovered && is_tree_screen(state.menu);
    m_seen = true;
    m_menu = state.menu;
    m_hovered = state.hovered;
    if (arrived) {
        m_speech.announce(opening(state), Mode::interrupt);
    } else if (moved) {
        m_speech.announce(hovered_entry(state), Mode::interrupt);
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
    add_sentence(out, description(state));
    return out;
}

std::string MenuSpeech::description(const A11yMenuState& state) {
    std::string where =
        "[a11y] menu " + std::to_string(state.menu) + " entry " + std::to_string(state.hovered);
    if (state.pc_description != nullptr) {
        return spoken_text(state.pc_description);
    }
    if (state.description == nullptr) {
        log_once(where + ": no description");
        return "";
    }
    DecodedText decoded = decode_game_text(state.description, m_text);
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
