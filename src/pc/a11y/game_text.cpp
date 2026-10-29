/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_text.hpp"
#include <array>
#include <cstdio>
#include <cstring>

namespace a11y {
namespace {

/* The font atlas's glyphs in atlas order: glyph code 0x2000 + i on NTSC-U.
 * Derived from the game's two encoder tables in hsd_3A76.c, lbl_8040C8C0
 * (entry -> Shift-JIS) and HSD_SisLib_8040C680 (entry -> glyph code), which
 * pair up one to one over all 287 glyphs; each Shift-JIS character decoded as
 * CP932, with full-width ASCII, the ideographic space and the curly quote and
 * apostrophe folded to plain ASCII, the forms the encoder makes of them. */
// clang-format off
constexpr std::array<const char*, 287> k_atlas = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
    "ぁ", "あ", "ぃ", "い", "ぅ", "う", "ぇ", "え", "ぉ", "お",
    "か", "が", "き", "ぎ", "く", "ぐ", "け", "げ", "こ", "ご",
    "さ", "ざ", "し", "じ", "す", "ず", "せ", "ぜ", "そ", "ぞ",
    "た", "だ", "ち", "ぢ", "っ", "つ", "づ", "て", "で", "と", "ど",
    "な", "に", "ぬ", "ね", "の",
    "は", "ば", "ぱ", "ひ", "び", "ぴ", "ふ", "ぶ", "ぷ", "へ", "べ", "ぺ", "ほ", "ぼ", "ぽ",
    "ま", "み", "む", "め", "も", "ゃ", "や", "ゅ", "ゆ", "ょ", "よ",
    "ら", "り", "る", "れ", "ろ", "ゎ", "わ", "を", "ん",
    "ァ", "ア", "ィ", "イ", "ゥ", "ウ", "ェ", "エ", "ォ", "オ",
    "カ", "ガ", "キ", "ギ", "ク", "グ", "ケ", "ゲ", "コ", "ゴ",
    "サ", "ザ", "シ", "ジ", "ス", "ズ", "セ", "ゼ", "ソ", "ゾ",
    "タ", "ダ", "チ", "ヂ", "ッ", "ツ", "ヅ", "テ", "デ", "ト", "ド",
    "ナ", "ニ", "ヌ", "ネ", "ノ",
    "ハ", "バ", "パ", "ヒ", "ビ", "ピ", "フ", "ブ", "プ", "ヘ", "ベ", "ペ", "ホ", "ボ", "ポ",
    "マ", "ミ", "ム", "メ", "モ", "ャ", "ヤ", "ュ", "ユ", "ョ", "ヨ",
    "ラ", "リ", "ル", "レ", "ロ", "ヮ", "ワ", "ヲ", "ン", "ヴ", "ヵ", "ヶ",
    " ", "、", "。", ",", ".", "・", ":", ";", "?", "!", "^", "_", "ー", "/", "~", "|",
    "'", "\"", "(", ")", "[", "]", "{", "}", "+", "-", "×", "=", "<", ">", "￥", "$",
    "%", "#", "&", "*", "@",
    "扱", "押", "軍", "源", "個", "込", "指", "示", "取", "書", "詳", "人", "生", "説",
    "体", "団", "電", "読", "発", "抜", "閉", "本", "明",
};
// clang-format on
static_assert(k_atlas.back() != nullptr, "one entry per atlas glyph");

/* The PAL atlas starts with the printable ASCII characters from '!' in order;
 * its accented letters after them are not mapped yet. */
constexpr int k_pal_ascii_glyphs = '~' - '!' + 1;

/* Guards against a string that never ends or jumps in a circle. */
constexpr int k_max_steps = 8192;
constexpr int k_max_call_depth = 8;

std::uint32_t read_u32(const std::uint8_t* p) {
    return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) |
           std::uint32_t{p[3]};
}

void append_unknown(DecodedText& out, unsigned code, int digits) {
    char buf[24];
    std::snprintf(buf, sizeof buf, "{glyph %0*X}", digits, code);
    out.text += buf;
    out.unknown_glyphs++;
}

/* Spaces at the start of a line are only layout. */
void append_text(DecodedText& out, const char* text) {
    bool line_start = out.text.empty() || out.text.back() == '\n';
    if (line_start && std::strcmp(text, " ") == 0) {
        return;
    }
    out.text += text;
}

/* One line break, however many break commands the string has in a row. */
void append_break(DecodedText& out) {
    while (!out.text.empty() && out.text.back() == ' ') {
        out.text.pop_back();
    }
    if (!out.text.empty() && out.text.back() != '\n') {
        out.text += '\n';
    }
}

}  // namespace

/* Mirrors the command walk of the game's renderer, HSD_SisLib_803A84BC in
 * hsd_3A76.c: only commands that change which characters come next matter
 * here, the rest (colour, scale, spacing, alignment, timing) are skipped by
 * their operand size. */
DecodedText decode_game_text(const std::uint8_t* bytes, const GameTextSource& source) {
    DecodedText out;
    std::array<const std::uint8_t*, k_max_call_depth> returns{};
    int depth = 0;
    const std::uint8_t* p = bytes;
    if (p == nullptr) {
        out.malformed = true;
        return out;
    }
    for (int step = 0; step < k_max_steps; step++) {
        std::uint8_t op = *p;
        if (source.pal && op == 0x20) {
            op = 26; /* PAL's one-byte space; see sis_opcode */
        }
        switch (op) {
        case 0: /* end, or return from a call */
            if (depth == 0) {
                append_break(out);
                if (!out.text.empty()) {
                    out.text.pop_back();
                }
                return out;
            }
            p = returns[--depth];
            continue;
        case 1: /* new page */
        case 2:
        case 3: /* line break */
        case 4: /* wait for a button */
            append_break(out);
            p += 1;
            continue;
        case 7: /* start a line at a position */
            append_break(out);
            p += 5;
            continue;
        case 8: /* jump */
        case 9: /* call */
        {
            const std::uint8_t* target =
                source.resolve != nullptr ? source.resolve(read_u32(p + 1)) : nullptr;
            if (target == nullptr || (op == 9 && depth == k_max_call_depth)) {
                out.malformed = true;
                return out;
            }
            if (op == 9) {
                returns[depth++] = p + 5;
            }
            p = target;
            continue;
        }
        case 5:
            p += 3;
            continue;
        case 12:
            p += 4;
            continue;
        case 6:
        case 10:
        case 14:
            p += 5;
            continue;
        case 11:
        case 13:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
            p += 1;
            continue;
        case 26: /* space */
            append_text(out, " ");
            p += 1;
            continue;
        default:
            break;
        }
        if (op < 0x20) {
            out.malformed = true;
            return out;
        }
        if (source.pal) {
            int index = op - 0x21;
            if (index < k_pal_ascii_glyphs) {
                const char ascii[2] = {static_cast<char>(op), '\0'};
                append_text(out, ascii);
            } else {
                append_unknown(out, op, 2);
            }
            p += 1;
            continue;
        }
        unsigned code = (unsigned{p[0]} << 8) | p[1];
        if (code >= 0x2000 && code - 0x2000 < k_atlas.size()) {
            append_text(out, k_atlas[code - 0x2000]);
        } else {
            /* 0x4000 and up: a glyph from the screen's own font file. */
            append_unknown(out, code, 4);
        }
        p += 2;
    }
    out.malformed = true;
    return out;
}

namespace {

/* Shift-JIS row 1's symbols from 0x8140 that have an ASCII form, folded as
 * the game's encoder folds them, and the few kana marks the name keyboard
 * offers. */
const char* shift_jis_symbol(unsigned code) {
    switch (code) {
    case 0x8140:
        return " ";
    case 0x8141:
        return "、";
    case 0x8142:
        return "。";
    case 0x8143:
        return ",";
    case 0x8144:
        return ".";
    case 0x8145:
        return "・";
    case 0x8146:
        return ":";
    case 0x8147:
        return ";";
    case 0x8148:
        return "?";
    case 0x8149:
        return "!";
    case 0x814F:
        return "^";
    case 0x8151:
        return "_";
    case 0x815B:
        return "ー";
    case 0x815E:
        return "/";
    case 0x8160:
        return "~";
    case 0x8162:
        return "|";
    case 0x8165:
    case 0x8166:
        return "'";
    case 0x8167:
    case 0x8168:
        return "\"";
    case 0x8169:
        return "(";
    case 0x816A:
        return ")";
    case 0x816D:
        return "[";
    case 0x816E:
        return "]";
    case 0x816F:
        return "{";
    case 0x8170:
        return "}";
    case 0x817B:
        return "+";
    case 0x817C:
        return "-";
    case 0x817E:
        return "×";
    case 0x8181:
        return "=";
    case 0x8183:
        return "<";
    case 0x8184:
        return ">";
    case 0x818F:
        return "￥";
    case 0x8190:
        return "$";
    case 0x8193:
        return "%";
    case 0x8194:
        return "#";
    case 0x8195:
        return "&";
    case 0x8196:
        return "*";
    case 0x8197:
        return "@";
    default:
        return nullptr;
    }
}

void append_utf8(std::string& out, unsigned point) {
    if (point < 0x80) {
        out += static_cast<char>(point);
    } else if (point < 0x800) {
        out += static_cast<char>(0xC0 | (point >> 6));
        out += static_cast<char>(0x80 | (point & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (point >> 12));
        out += static_cast<char>(0x80 | ((point >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (point & 0x3F));
    }
}

/* One two-byte Shift-JIS character; false for one with no mapping here. */
bool append_shift_jis(std::string& out, unsigned code) {
    if (const char* symbol = shift_jis_symbol(code)) {
        out += symbol;
    } else if (code >= 0x824F && code <= 0x8258) {
        out += static_cast<char>('0' + (code - 0x824F));
    } else if (code >= 0x8260 && code <= 0x8279) {
        out += static_cast<char>('A' + (code - 0x8260));
    } else if (code >= 0x8281 && code <= 0x829A) {
        out += static_cast<char>('a' + (code - 0x8281));
    } else if (code >= 0x829F && code <= 0x82F1) {
        append_utf8(out, 0x3041 + (code - 0x829F)); /* hiragana */
    } else if (code >= 0x8340 && code <= 0x837E) {
        append_utf8(out, 0x30A1 + (code - 0x8340)); /* katakana */
    } else if (code >= 0x8380 && code <= 0x8396) {
        append_utf8(out, 0x30E0 + (code - 0x8380)); /* katakana past the gap */
    } else {
        return false;
    }
    return true;
}

}  // namespace

DecodedText decode_shift_jis(const char* text) {
    DecodedText out;
    if (text == nullptr) {
        out.malformed = true;
        return out;
    }
    const auto* p = reinterpret_cast<const unsigned char*>(text);
    while (*p != 0) {
        unsigned lead = *p;
        bool two_bytes = (lead >= 0x81 && lead <= 0x9F) || (lead >= 0xE0 && lead <= 0xFC);
        if (!two_bytes) {
            if (lead < 0x80) {
                out.text += static_cast<char>(lead);
            } else {
                char buf[24];
                std::snprintf(buf, sizeof buf, "{sjis %02X}", lead);
                out.text += buf;
                out.unknown_glyphs++;
            }
            p += 1;
            continue;
        }
        if (p[1] == 0) {
            out.malformed = true;
            return out;
        }
        unsigned code = (lead << 8) | p[1];
        if (!append_shift_jis(out.text, code)) {
            char buf[24];
            std::snprintf(buf, sizeof buf, "{sjis %04X}", code);
            out.text += buf;
            out.unknown_glyphs++;
        }
        p += 2;
    }
    return out;
}

std::string one_line(std::string text) {
    for (std::size_t at = text.find('\n'); at != std::string::npos; at = text.find('\n', at)) {
        text.replace(at, 1, " / ");
    }
    return text;
}

std::string decode_trouble(const DecodedText& decoded) {
    if (decoded.unknown_glyphs == 0 && !decoded.malformed) {
        return "";
    }
    return "\"" + one_line(decoded.text) + "\"" +
           (decoded.unknown_glyphs > 0 ? ", unknown glyphs left out" : "") +
           (decoded.malformed ? ", malformed" : "");
}

std::string spoken_text(std::string_view text) {
    constexpr std::string_view kGlyphMark = "{glyph ";
    constexpr std::string_view kShiftJisMark = "{sjis ";
    std::string out;
    for (std::size_t i = 0; i < text.size(); i++) {
        if (text.substr(i).starts_with(kGlyphMark) || text.substr(i).starts_with(kShiftJisMark)) {
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

}  // namespace a11y
