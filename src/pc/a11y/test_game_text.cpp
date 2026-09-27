/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The game text decoder against byte code written by hand, in the layout the
 * game's own encoder (HSD_SisLib_803A67EC) and string archives use. */
#include "game_text.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

/* NTSC-U glyph code of an ASCII letter, digit, space or full stop. */
std::uint16_t glyph(char c) {
    if (c >= '0' && c <= '9') {
        return 0x2000 + (c - '0');
    }
    if (c >= 'A' && c <= 'Z') {
        return 0x200A + (c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
        return 0x2024 + (c - 'a');
    }
    if (c == ' ') {
        return 0x20E3;
    }
    assert(c == '.');
    return 0x20E7;
}

void add_text(Bytes& out, const char* text) {
    for (; *text != '\0'; text++) {
        std::uint16_t code = glyph(*text);
        out.push_back(static_cast<std::uint8_t>(code >> 8));
        out.push_back(static_cast<std::uint8_t>(code));
    }
}

void add_slot(Bytes& out, std::uint8_t op, std::uint32_t slot) {
    out.push_back(op);
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(slot >> shift));
    }
}

/* Jump targets for the tests: slot n is s_targets[n - 1]. */
std::vector<const Bytes*> s_targets;

const std::uint8_t* resolve(std::uint32_t slot) {
    if (slot == 0 || slot > s_targets.size()) {
        return nullptr;
    }
    return s_targets[slot - 1]->data();
}

a11y::GameTextSource ntsc() {
    a11y::GameTextSource source;
    source.resolve = resolve;
    return source;
}

void skips_formatting_commands() {
    /* Scale, colour, then the text and the commands that pop them. */
    Bytes b = {0x0E, 0x00, 0x80, 0x00, 0x80, 0x0C, 0xFF, 0xFF, 0xFF};
    add_text(b, "Hi there.");
    b.insert(b.end(), {0x0D, 0x0F, 0x00});
    a11y::DecodedText d = a11y::decode_game_text(b.data(), ntsc());
    assert(d.text == "Hi there.");
    assert(d.unknown_glyphs == 0);
    assert(!d.malformed);
}

void line_commands_become_one_break() {
    /* A positioned line, two line breaks in a row with spaces around them, a
     * trailing break. */
    Bytes b = {0x07, 0x00, 0x10, 0x00, 0x20};
    add_text(b, "Line 1 ");
    b.insert(b.end(), {0x03, 0x03});
    add_text(b, " Line 2");
    b.insert(b.end(), {0x03, 0x00});
    a11y::DecodedText d = a11y::decode_game_text(b.data(), ntsc());
    assert(d.text == "Line 1\nLine 2");
}

void follows_calls_and_jumps() {
    Bytes called;
    add_text(called, "B");
    called.push_back(0x00);
    Bytes jumped;
    add_text(jumped, "D");
    jumped.push_back(0x00);
    s_targets = {&called, &jumped};

    Bytes b;
    add_text(b, "A");
    add_slot(b, 0x09, 1);
    add_text(b, "C");
    add_slot(b, 0x08, 2);
    add_text(b, "never");
    b.push_back(0x00);
    a11y::DecodedText d = a11y::decode_game_text(b.data(), ntsc());
    assert(d.text == "ABCD");
    assert(!d.malformed);
    s_targets.clear();
}

void font_glyphs_are_marked() {
    Bytes b;
    add_text(b, "A");
    b.insert(b.end(), {0x40, 0x05, 0x00});
    a11y::DecodedText d = a11y::decode_game_text(b.data(), ntsc());
    assert(d.text == "A{glyph 4005}");
    assert(d.unknown_glyphs == 1);
}

void kana_and_kanji_decode() {
    /* The first hiragana and the last glyph of the atlas. */
    Bytes b = {0x20, 0x3E, 0x21, 0x1E, 0x00};
    a11y::DecodedText d = a11y::decode_game_text(b.data(), ntsc());
    assert(d.text == "ぁ明");
}

void pal_uses_one_byte_per_glyph() {
    a11y::GameTextSource pal = ntsc();
    pal.pal = true;
    Bytes b = {0x0C, 0xFF, 0xFF, 0xFF, 'H', 'i', 0x20, 't', 'o', '!', 0x80, 0x0D, 0x00};
    a11y::DecodedText d = a11y::decode_game_text(b.data(), pal);
    assert(d.text == "Hi to!{glyph 80}");
    assert(d.unknown_glyphs == 1);
}

void stops_on_what_it_cannot_follow() {
    Bytes unknown_op;
    add_text(unknown_op, "A");
    unknown_op.insert(unknown_op.end(), {0x1B, 0x00});
    a11y::DecodedText d = a11y::decode_game_text(unknown_op.data(), ntsc());
    assert(d.malformed && d.text == "A");

    Bytes nowhere;
    add_slot(nowhere, 0x08, 7);
    assert(a11y::decode_game_text(nowhere.data(), ntsc()).malformed);

    Bytes loop;
    add_slot(loop, 0x08, 1);
    s_targets = {&loop};
    assert(a11y::decode_game_text(loop.data(), ntsc()).malformed);
    s_targets.clear();

    assert(a11y::decode_game_text(nullptr, ntsc()).malformed);
}

}  // namespace

int main() {
    skips_formatting_commands();
    line_commands_become_one_break();
    follows_calls_and_jumps();
    font_glyphs_are_marked();
    kana_and_kanji_decode();
    pal_uses_one_byte_per_glyph();
    stops_on_what_it_cannot_follow();
    std::cout << "game_text: all tests passed\n";
    return 0;
}
