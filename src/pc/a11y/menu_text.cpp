/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_text.hpp"
#include "game_access.h"
#include "game_text.hpp"
#include "pc/pc.h"
#include "pc/region.h"
#include "speech.hpp"
#include <cstdlib>
#include <cstring>
#include <string>

namespace a11y {
namespace {

/* The main menu's strings sit in font slot 0 (mnMain_Scene_OnEnter in
 * mnmain.c loads SdMenu there). */
constexpr int k_menu_font = 0;
/* Far past the few hundred strings of any table, in case a table's end is
 * not followed by a word that resolves nowhere. */
constexpr int k_max_strings = 4096;

bool s_dumped = false;

GameTextSource game_source() {
    GameTextSource source;
    source.pal = pc_region_pal;
    source.resolve = a11y_game_resolve;
    return source;
}

/* One log line per string, so its breaks are shown as " / ". */
std::string one_line(const DecodedText& decoded) {
    std::string line = decoded.text;
    for (std::size_t at = line.find('\n'); at != std::string::npos; at = line.find('\n', at)) {
        line.replace(at, 1, " / ");
    }
    if (decoded.malformed) {
        line += " {malformed}";
    }
    return line;
}

bool log_enabled() {
    static const bool enabled = config_from_environment().log;
    return enabled;
}

void dump_table() {
    const char* symbol = a11y_game_font_symbol(k_menu_font);
    GameTextSource source = game_source();
    int count = 0;
    int unknown = 0;
    int malformed = 0;
    for (; count < k_max_strings; count++) {
        const std::uint8_t* entry = a11y_game_string(k_menu_font, count);
        if (entry == nullptr) {
            break;
        }
        DecodedText decoded = decode_game_text(entry, source);
        unknown += decoded.unknown_glyphs;
        malformed += decoded.malformed ? 1 : 0;
        pc_log_line("[a11y] game text %s %d: \"%s\"", symbol, count, one_line(decoded).c_str());
    }
    pc_log_line("[a11y] game text %s: %d strings, %d unknown glyphs, %d malformed", symbol, count,
        unknown, malformed);
}

}  // namespace

void menu_text_dump_once() {
    static const bool wanted = [] {
        const char* dump = std::getenv("MELEE_A11Y_TEXT_DUMP");
        return dump != nullptr && std::strcmp(dump, "0") != 0;
    }();
    if (!wanted || s_dumped || !log_enabled() || a11y_game_font_symbol(k_menu_font) == nullptr) {
        return;
    }
    s_dumped = true;
    dump_table();
}

}  // namespace a11y
