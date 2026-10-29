/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Unlock notice speech against the hook's calls as the unlock notice scene
 * makes them, over a fake screen reader bridge and a hand-written prize
 * table: each notice said and interrupting, the key on the first of a
 * visit, a trophy notice, the prize font's é and an undecodable text; and
 * the New challenger line, with and without the challenged player's label,
 * and for a kind with no name. Links unlock_speech.cpp, css_names.cpp,
 * game_text.cpp and speech.cpp only, so this file supplies the pc_log_line
 * that main.c normally provides. */
#include "unlock_speech.hpp"
#include "speech.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<std::string> s_lines;

struct Output {
    std::string text;
    bool interrupt;

    bool operator==(const Output&) const = default;
};

using Outputs = std::vector<Output>;

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    explicit FakeBridge(Outputs& outputs) : m_outputs(outputs) {}

    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view utf8, bool interrupt) override {
        m_outputs.push_back(Output{std::string(utf8), interrupt});
        return "";
    }

private:
    Outputs& m_outputs;
};

using Bytes = std::vector<std::uint8_t>;

/* PAL game text, one byte a glyph, so ASCII stays itself; '\n' is a line
 * break. */
Bytes game_text(const char* text) {
    Bytes out;
    for (; *text != '\0'; text++) {
        out.push_back(*text == '\n' ? std::uint8_t{0x03} : static_cast<std::uint8_t>(*text));
    }
    out.push_back(0x00);
    return out;
}

/* NTSC-U game text of letters, the é of the prize font written as '@'. */
Bytes ntsc_text(const char* text) {
    Bytes out;
    for (; *text != '\0'; text++) {
        char c = *text;
        std::uint16_t code = 0x200A + (c - 'A');
        if (c == '@') {
            code = 0x4000;
        } else if (c >= 'a' && c <= 'z') {
            code = 0x2024 + (c - 'a');
        }
        out.push_back(static_cast<std::uint8_t>(code >> 8));
        out.push_back(static_cast<std::uint8_t>(code));
    }
    out.push_back(0x00);
    return out;
}

/* SdPrize's strings as the US disc has them, line breaks and the double
 * spaces included; 0x4A as a trophy notice leaves it. */
const std::map<int, Bytes>& pal_table() {
    static const std::map<int, Bytes> strings = {
        {0x18, game_text("1-P All-Star mode is now open!\nBetter practice up!")},
        {0x1A, game_text("Random Stage Select is now\navailable in Additional Rules!")},
        {0x20, game_text("You've unlocked all playable\ncharacters!  Get to fighting!")},
        {0x4A, game_text("You got\nthe Kirby Hat 4 trophy!")},
        /* A byte the decoder does not understand after "Bad". */
        {0x30, Bytes{'B', 'a', 'd', 0x1B, 0x00}},
    };
    return strings;
}

const std::map<int, Bytes>& ntsc_table() {
    static const std::map<int, Bytes> strings = {{0x10, ntsc_text("Pok@mon")}};
    return strings;
}

const std::uint8_t* find(const std::map<int, Bytes>& table, int number) {
    auto it = table.find(number);
    return it == table.end() ? nullptr : it->second.data();
}

const std::uint8_t* pal_strings(int number) {
    return find(pal_table(), number);
}

const std::uint8_t* ntsc_strings(int number) {
    return find(ntsc_table(), number);
}

constexpr int kTrophy = 0x4A;
const std::string kAllStar = "1-P All-Star mode is now open! Better practice up!";
const std::string kRandomStage = "Random Stage Select is now available in Additional Rules!";
const std::string kCharacters = "You've unlocked all playable characters! Get to fighting!";
const std::string kKey = "A for next.";

struct Fixture {
    Outputs outputs;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(outputs)};
    a11y::UnlockSpeech reader;

    explicit Fixture(bool pal = true)
        : reader(speech, a11y::GameTextSource{pal, nullptr}, pal ? pal_strings : ntsc_strings) {
        speech.init();
        s_lines.clear();
        reader.visit_started();
    }

    /* The notice the scene sets up; returns what was said, clearing it. */
    Outputs notice(int text_id) {
        reader.notice(text_id);
        Outputs said = std::move(outputs);
        outputs.clear();
        return said;
    }
};

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

void each_notice_is_said_and_interrupts() {
    Fixture f;
    assert(f.notice(0x18) == (Outputs{{kAllStar + " " + kKey, true}}));
    assert(f.notice(0x1A) == (Outputs{{kRandomStage, true}}));
    /* The disc's double space is said as one. */
    assert(f.notice(0x20) == (Outputs{{kCharacters, true}}));
}

void the_key_on_the_first_notice_of_a_visit_only() {
    Fixture f;
    assert(f.notice(0x1A) == (Outputs{{kRandomStage + " " + kKey, true}}));
    assert(f.notice(0x18) == (Outputs{{kAllStar, true}}));
    assert(f.notice(0x18) == (Outputs{{kAllStar, true}}));
    /* The next visit's first says it again. */
    f.reader.visit_started();
    assert(f.notice(0x20) == (Outputs{{kCharacters + " " + kKey, true}}));
    assert(f.notice(0x1A) == (Outputs{{kRandomStage, true}}));
}

void a_trophy_notice_says_the_composed_text() {
    Fixture f;
    f.notice(0x18);
    assert(f.notice(kTrophy) == (Outputs{{"You got the Kirby Hat 4 trophy!", true}}));
}

void the_prize_fonts_e_is_said() {
    Fixture f(false);
    assert(f.notice(0x10) == (Outputs{{"Pokémon. " + kKey, true}}));
    assert(s_lines.size() == 1);
    assert(s_lines[0] == "[a11y] speak interrupt: \"Pokémon. " + kKey + "\"");
}

void an_undecodable_text_is_logged_once_and_the_notice_still_said() {
    Fixture f;
    /* No such string: the fork's words stand in. */
    assert(f.notice(0x60) == (Outputs{{"Unlock notice. " + kKey, true}}));
    assert(count_logged("[a11y] unlock notice: no game text for text 96") == 1);
    assert(f.notice(0x60) == (Outputs{{"Unlock notice.", true}}));
    assert(count_logged("[a11y] unlock notice: no game text for text 96") == 1);
    /* A string the decoder stops in: what came before it is said. */
    assert(f.notice(0x30) == (Outputs{{"Bad", true}}));
    f.notice(0x30);
    assert(std::count_if(s_lines.begin(), s_lines.end(), [](const std::string& line) {
        return line.starts_with("[a11y] unlock notice: text 48 ");
    }) == 1);
}

/* The challenger data as the New challenger screen finds it. */
A11yChallenger challenger(A11yCharacter character, int port, int humans, const char* tag = "") {
    A11yChallenger out{};
    out.character = character;
    out.kind = character;
    out.port = port;
    out.humans = humans;
    std::strncpy(out.tag, tag, sizeof(out.tag) - 1);
    return out;
}

const std::string kWarning = "Warning! Challenger approaching! ";

void the_challenger_is_named_with_the_key() {
    Fixture f;
    f.reader.challenger(challenger(A11Y_CKind_Purin, 0, 1));
    assert((f.outputs ==
            Outputs{{kWarning + "A new foe has appeared: Jigglypuff. A to fight.", true}}));
    f.outputs.clear();
    /* A single-player mode's one human keeps no label, name tag or not. */
    f.reader.challenger(challenger(A11Y_CKind_Mewtwo, 2, 1, "Bob"));
    assert(
        (f.outputs == Outputs{{kWarning + "A new foe has appeared: Mewtwo. A to fight.", true}}));
    assert(s_lines.size() == 2);
}

void two_humans_hear_whom_it_challenges() {
    Fixture f;
    f.reader.challenger(challenger(A11Y_CKind_Purin, 1, 2));
    assert((f.outputs ==
            Outputs{{kWarning + "A new foe has appeared: Jigglypuff. P2, A to fight.", true}}));
    f.outputs.clear();
    /* The name tag where the challenged player has one. */
    f.reader.challenger(challenger(A11Y_CKind_GameWatch, 3, 4, "Bob"));
    assert((
        f.outputs ==
        Outputs{{kWarning + "A new foe has appeared: Mr. Game & Watch. Bob, A to fight.", true}}));
}

void an_unknown_challenger_is_left_out_and_logged_once() {
    Fixture f;
    A11yChallenger boss = challenger(A11Y_NO_CHARACTER, 0, 2);
    boss.kind = 0x1B;
    f.reader.challenger(boss);
    f.reader.challenger(boss);
    const Output said{kWarning + "A new foe has appeared! P1, A to fight.", true};
    assert((f.outputs == Outputs{said, said}));
    assert(count_logged("[a11y] new challenger: no name for character 27") == 1);
}

}  // namespace

extern "C" void pc_log_line(const char* fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    s_lines.emplace_back(line);
}

int main() {
    each_notice_is_said_and_interrupts();
    the_key_on_the_first_notice_of_a_visit_only();
    a_trophy_notice_says_the_composed_text();
    the_prize_fonts_e_is_said();
    an_undecodable_text_is_logged_once_and_the_notice_still_said();
    the_challenger_is_named_with_the_key();
    two_humans_hear_whom_it_challenges();
    an_unknown_challenger_is_left_out_and_logged_once();
    std::cout << "unlock_speech: all tests passed\n";
    return 0;
}
