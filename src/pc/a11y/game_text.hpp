/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
#include <string>

namespace a11y {

/* Where the bytes of game text live and how they encode glyphs. The decoder
 * itself touches no game state, so tests can hand it bytes of their own. */
struct GameTextSource {
    /* PAL discs spend one byte per glyph, NTSC-U discs two. */
    bool pal = false;
    /* Turns a jump target (a 32-bit disc pointer, as stored in the byte code)
     * into a host address, or nullptr when it resolves nowhere. */
    const std::uint8_t* (*resolve)(std::uint32_t slot) = nullptr;
};

struct DecodedText {
    /* UTF-8. Line breaks are '\n'; glyphs with no known character appear as
     * "{glyph XXXX}" so an experiment can count them. */
    std::string text;
    int unknown_glyphs = 0;
    /* Set when decoding stopped at a byte it does not understand, a jump that
     * resolved nowhere or a runaway string; text holds what came before. */
    bool malformed = false;
};

/* Reads one string of the game's text byte code (SIS, see
 * docs/a11y/native-menus.md) from its first byte to its end, following jumps
 * and calls into other strings. */
DecodedText decode_game_text(const std::uint8_t* bytes, const GameTextSource& source);

/* Decoded text for a log line, its breaks shown as " / " and its unknown
 * glyphs left marked. */
std::string one_line(std::string text);

}  // namespace a11y
