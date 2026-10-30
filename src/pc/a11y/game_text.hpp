/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace a11y {

/* Where the bytes of game text live and how they encode glyphs. The decoder
 * itself touches no game state, so tests can hand it bytes of their own. */
struct GameTextSource {
    /* PAL discs spend one byte per glyph, NTSC-U discs two. */
    bool pal = false;
    /* Turns a jump target (a 32-bit disc pointer, as stored in the byte code)
     * into a host address, or nullptr when it resolves nowhere. */
    const std::uint8_t* (*resolve)(std::uint32_t slot) = nullptr;
    /* NTSC-U glyphs from 0x4000 come from the screen's own font file, which
     * has no character codes: the characters read from context for the
     * table being decoded, from 0x4000 in order. Empty, or nullptr for a
     * glyph, leaves them unknown; a table's own are given only where they
     * are known to hold. */
    std::span<const char* const> font_glyphs = {};
};

/* SdPrize's font glyphs (the unlock notices, SIS_PrizeData, on the US
 * disc): 0x4000 is the é of "Pokémon", 0x4002 the numeral II. */
std::span<const char* const> prize_font_glyphs();

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

/* A Shift-JIS (CP932) C string, as the game keeps name tags: plain ASCII,
 * full-width letters, digits and the name keyboard's symbols folded to ASCII
 * as the game's encoder folds them, the ideographic space to a space, and
 * kana as kana. Any other character appears as "{sjis XXXX}" and counts as
 * an unknown glyph, which spoken_text leaves out; a lead byte with no trail
 * byte, or a null pointer, marks the text malformed. */
DecodedText decode_shift_jis(const char* text);

/* Decoded text for a log line, its breaks shown as " / " and its unknown
 * glyphs left marked. */
std::string one_line(std::string text);

/* What went wrong in decoding, for a log line: the text as one_line shows it
 * and what became of it ("\"Pok{glyph 4000}mon\", unknown glyphs left out");
 * empty when it decoded cleanly. */
std::string decode_trouble(const DecodedText& decoded);

/* Decoded text as one spoken line: the lines of wrapped text joined by a
 * space, the marks of unknown glyphs ("{glyph 4000}", "{sjis 889F}") left
 * out. */
std::string spoken_text(std::string_view text);

/* Game text by its first byte as one spoken line, the readers' shared step:
 * decoded, a decoding problem handed to log as "<what> <trouble>" (a reader's
 * LogOnce), and spoken as spoken_text has it. nullptr gives no words; a
 * reader that wants it logged checks first. */
template <typename Log>
std::string spoken_game_text(
    const std::uint8_t* bytes, const GameTextSource& source, Log& log, const std::string& what) {
    if (bytes == nullptr) {
        return "";
    }
    DecodedText decoded = decode_game_text(bytes, source);
    if (std::string trouble = decode_trouble(decoded); !trouble.empty()) {
        log(what + " " + trouble);
    }
    return spoken_text(decoded.text);
}

/* A human player's label (CONTEXT.md, "Player label"), the readers' shared
 * step: the name tag where the player has one with words in it, otherwise
 * "P" and the port, "P2" for port 1. tag is the Shift-JIS name tag, empty or
 * nullptr for none; a decoding problem is handed to log as "<what> name tag
 * <trouble>" (a reader's LogOnce). A computer player's label, "teammate"
 * and whether a tag counts (online each copy reads its own card) are the
 * caller's. */
template <typename Log>
std::string player_label(const char* tag, int port, Log& log, const std::string& what) {
    if (tag != nullptr && tag[0] != '\0') {
        DecodedText decoded = decode_shift_jis(tag);
        if (std::string trouble = decode_trouble(decoded); !trouble.empty()) {
            log(what + " name tag " + trouble);
        }
        if (std::string name = spoken_text(decoded.text); !name.empty()) {
            return name;
        }
    }
    return "P" + std::to_string(port + 1);
}

}  // namespace a11y
