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

/* The table's number for the string starting at bytes, or -1. */
int string_index(const std::uint8_t* bytes) {
    for (int i = 0; i < k_max_strings; i++) {
        const std::uint8_t* entry = a11y_game_string(k_menu_font, i);
        if (entry == nullptr) {
            break;
        }
        if (entry == bytes) {
            return i;
        }
    }
    return -1;
}

void dump_table() {
    const char* symbol = a11y_game_font_symbol(k_menu_font);
    if (symbol == nullptr) {
        return;
    }
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

void menu_description_shown(int menu_kind, int selection, const std::uint8_t* bytes) {
    if (!log_enabled()) {
        return;
    }
    const char* dump = std::getenv("MELEE_A11Y_TEXT_DUMP");
    if (!s_dumped && dump != nullptr && std::strcmp(dump, "0") != 0) {
        s_dumped = true;
        dump_table();
    }
    DecodedText decoded = decode_game_text(bytes, game_source());
    pc_log_line("[a11y] game text: menu %d entry %d, string %d: \"%s\"", menu_kind, selection,
        string_index(bytes), one_line(decoded).c_str());
}

}  // namespace a11y
