/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Memory card prompt speech against the hooks' calls as the memory card
 * scene makes them, over a fake screen reader bridge and a hand-written
 * message table: a prompt held for a frame, the moved default, the keys,
 * the choice moves, messages, queueing and an undecodable text. Links
 * memcard_speech.cpp, game_text.cpp and speech.cpp only, so this file
 * supplies the pc_log_line that main.c normally provides. */
#include "memcard_speech.hpp"
#include "speech.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
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

/* SdMsgBox's strings as the US disc has them, line breaks included. */
const std::map<int, Bytes>& table() {
    static const std::map<int, Bytes> strings = {
        {2, game_text("OK")},
        {3, game_text("Yes")},
        {4, game_text("No")},
        {5, game_text("Try Again")},
        {8, game_text("The Game Data on the Memory Card\nin Slot A is corrupted. Erase this\nand "
                      "create new Game Data?")},
        {9, game_text("Erasing the corrupted data...\nDo not touch the Memory Card\nor POWER "
                      "Button.")},
        {0xA, game_text("The Memory Card in Slot A\nhas no saved Game Data.\nCreate Game Data?")},
        {0xB, game_text("Creating new Game Data.\nDo not touch the Memory Card\nor POWER Button "
                        "!")},
        {0xC, game_text("Game Data has been created.\nThis game saves data automatically\nwhile "
                        "you play. Please do not\ntouch the Memory Card or POWER\nButton while "
                        "saving or copying.")},
        {0x16, game_text("There is no Memory Card\nin Slot A.")},
        {0x18, game_text("Continue without saving\nor loading Game Data?")},
        /* A byte the decoder does not understand after "Bad". */
        {0x30, Bytes{'B', 'a', 'd', 0x1B, 0x00}},
    };
    return strings;
}

/* Swapped by a test to take the choice names away. */
bool s_choice_names = true;

const std::uint8_t* strings(int number) {
    if (!s_choice_names && number >= 2 && number <= 5) {
        return nullptr;
    }
    auto it = table().find(number);
    return it == table().end() ? nullptr : it->second.data();
}

constexpr int kMessage = 0;
constexpr int kYesNo = 1;
constexpr int kOkRetry = 2;

const std::string kCreate = "The Memory Card in Slot A has no saved Game Data. Create Game Data?";
const std::string kContinue = "Continue without saving or loading Game Data?";
const std::string kKeys = "Left and Right choose, A confirms.";

struct Fixture {
    Outputs outputs;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(outputs)};
    a11y::MemcardSpeech reader{speech, a11y::GameTextSource{true, nullptr}, strings};

    Fixture() {
        speech.init();
        s_lines.clear();
        s_choice_names = true;
        reader.visit_started();
    }

    /* One frame of the scene: its start with the choice hovered, then the
     * prompt the frame shows, if any. Returns what was said, clearing it. */
    Outputs frame(int hovered, int kind = -1, int text_id = -1) {
        reader.frame(hovered);
        if (kind >= 0) {
            reader.prompt(kind, text_id);
        }
        Outputs said = std::move(outputs);
        outputs.clear();
        return said;
    }

    /* A prompt shown and held until it is said; returns what was said. */
    Outputs show(int kind, int text_id, int hovered = 0) {
        assert(frame(hovered, kind, text_id).empty());
        assert(frame(hovered).empty());
        return frame(hovered);
    }
};

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

void a_prompt_is_said_once_it_has_stayed_up_a_whole_frame() {
    Fixture f;
    /* The scene's first frames show nothing. */
    assert(f.frame(0).empty());
    assert(f.frame(0, kYesNo, 0xA).empty());
    /* Its next frame might still replace it. */
    assert(f.frame(0).empty());
    Outputs said = f.frame(0);
    assert(said.size() == 1);
    assert(said[0].text == kCreate + " Yes. " + kKeys);
    /* Once only. */
    for (int i = 0; i < 100; i++) {
        assert(f.frame(0).empty());
    }
}

void a_prompt_replaced_on_its_next_frame_is_never_said() {
    Fixture f;
    f.show(kYesNo, 0xA);
    /* Yes: "Creating new Game Data." for one frame, then the result. */
    assert(f.frame(0, kMessage, 0xB).empty());
    assert(f.frame(0, kMessage, 0xC).empty());
    assert(f.frame(0).empty());
    Outputs said = f.frame(0);
    assert(said.size() == 1);
    assert(said[0].text.starts_with("Game Data has been created."));
    for (const Output& output : said) {
        assert(output.text.find("Creating") == std::string::npos);
    }
}

void the_moved_default_is_the_choice_said() {
    Fixture f;
    /* A corrupted save: the game moves the default to No just after the
     * prompt, within the same frame. */
    f.reader.frame(0);
    f.reader.prompt(kYesNo, 8);
    assert(f.frame(1).empty());
    Outputs said = f.frame(1);
    assert(said.size() == 1);
    assert(said[0].text ==
           "The Game Data on the Memory Card in Slot A is corrupted. Erase this and create new "
           "Game Data? No. " +
               kKeys);
    /* Nor is the default said again as a move. */
    assert(f.frame(1).empty());
}

void a_choice_moved_before_the_prompt_is_said_is_said_with_it() {
    Fixture f;
    assert(f.frame(0, kYesNo, 0xA).empty());
    assert(f.frame(0).empty());
    /* Right during the held frame. */
    Outputs said = f.frame(1);
    assert(said.size() == 1);
    assert(said[0].text == kCreate + " No. " + kKeys);
}

void the_keys_on_the_first_prompt_with_a_choice_only() {
    Fixture f;
    /* A message first: its key, not the choice keys. */
    Outputs none = f.show(kMessage, 0x16);
    assert(none.size() == 1 && none[0].text == "There is no Memory Card in Slot A. A to continue.");
    Outputs first = f.show(kOkRetry, 0x18);
    assert(first.size() == 1 && first[0].text == kContinue + " OK. " + kKeys);
    Outputs second = f.show(kYesNo, 0xA);
    assert(second.size() == 1 && second[0].text == kCreate + " Yes.");
    Outputs third = f.show(kOkRetry, 0x18);
    assert(third.size() == 1 && third[0].text == kContinue + " OK.");
    /* Nor in a later visit of the same game run. */
    f.reader.visit_started();
    Outputs later = f.show(kYesNo, 0xA);
    assert(later.size() == 1 && later[0].text == kCreate + " Yes.");
}

void a_choice_move_is_said_alone() {
    Fixture f;
    f.show(kYesNo, 0xA);
    Outputs no = f.frame(1);
    assert(no == (Outputs{{"No", true}}));
    assert(f.frame(1).empty());
    Outputs yes = f.frame(0);
    assert(yes == (Outputs{{"Yes", true}}));
    f.show(kOkRetry, 0x18);
    assert(f.frame(1) == (Outputs{{"Try Again", true}}));
    assert(f.frame(0) == (Outputs{{"OK", true}}));
}

void a_press_at_the_end_says_nothing() {
    Fixture f;
    f.show(kYesNo, 0xA);
    /* Left at Yes leaves it on Yes. */
    for (int i = 0; i < 10; i++) {
        assert(f.frame(0).empty());
    }
    f.frame(1);
    for (int i = 0; i < 10; i++) {
        assert(f.frame(1).empty());
    }
}

void a_message_says_its_text_and_the_key_every_time() {
    Fixture f;
    Outputs first = f.show(kMessage, 0xC);
    assert(first.size() == 1);
    assert(first[0].text ==
           "Game Data has been created. This game saves data automatically while you play. "
           "Please do not touch the Memory Card or POWER Button while saving or copying. A to "
           "continue.");
    Outputs again = f.show(kMessage, 0x16);
    assert(
        again.size() == 1 && again[0].text == "There is no Memory Card in Slot A. A to continue.");
    Outputs third = f.show(kMessage, 0x16);
    assert(third.size() == 1 && third[0].text == again[0].text);
}

void a_message_says_nothing_of_the_choice() {
    Fixture f;
    f.show(kMessage, 0x16, 0);
    /* The scene keeps its last choice through a message; nothing moves. */
    assert(f.frame(1).empty());
    assert(f.frame(0).empty());
}

void the_first_prompt_of_the_run_queues_and_later_ones_interrupt() {
    Fixture f;
    Outputs first = f.show(kYesNo, 0xA);
    assert(first.size() == 1 && !first[0].interrupt);
    Outputs move = f.frame(1);
    assert(move.size() == 1 && move[0].interrupt);
    /* A on No: the frame starts on No, and the next prompt puts the choice
     * back on the left. */
    assert(f.frame(1, kOkRetry, 0x18).empty());
    assert(f.frame(0).empty());
    Outputs second = f.frame(0);
    assert(second.size() == 1 && second[0].interrupt);
    /* A later visit of the same run interrupts too. */
    f.reader.visit_started();
    Outputs later = f.show(kMessage, 0x16);
    assert(later.size() == 1 && later[0].interrupt);
}

void a_new_visit_forgets_the_last_prompt() {
    Fixture f;
    f.show(kYesNo, 0xA);
    f.frame(1);
    f.reader.visit_started();
    /* The scene's first frames, with its choice back on the left, say
     * nothing until a prompt shows. */
    assert(f.frame(0).empty());
    assert(f.frame(0).empty());
}

void the_scene_left_with_a_prompt_unsaid_says_nothing() {
    Fixture f;
    assert(f.frame(0, kMessage, 9).empty());
    f.reader.visit_started();
    assert(f.frame(0).empty());
    assert(f.frame(0).empty());
}

void an_undecodable_text_is_left_out_and_logged_once() {
    Fixture f;
    /* No such string: the prompt is said without its text. */
    Outputs missing = f.show(kYesNo, 0x40);
    assert(missing.size() == 1 && missing[0].text == "Yes. " + kKeys);
    assert(count_logged("[a11y] memory card: no game text for text 64") == 1);
    f.show(kYesNo, 0x40);
    assert(count_logged("[a11y] memory card: no game text for text 64") == 1);
    /* A string the decoder stops in: what came before it is said. */
    Outputs bad = f.show(kMessage, 0x30);
    assert(bad.size() == 1 && bad[0].text == "Bad. A to continue.");
    assert(std::count_if(s_lines.begin(), s_lines.end(), [](const std::string& line) {
        return line.starts_with("[a11y] memory card: text 48 ");
    }) == 1);
}

void missing_choice_names_fall_back_to_the_forks() {
    Fixture f;
    s_choice_names = false;
    Outputs said = f.show(kOkRetry, 0x18);
    assert(said.size() == 1 && said[0].text == kContinue + " OK. " + kKeys);
    assert(f.frame(1) == (Outputs{{"Try Again", true}}));
    assert(count_logged("[a11y] memory card: no game text for text 5") == 1);
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
    a_prompt_is_said_once_it_has_stayed_up_a_whole_frame();
    a_prompt_replaced_on_its_next_frame_is_never_said();
    the_moved_default_is_the_choice_said();
    a_choice_moved_before_the_prompt_is_said_is_said_with_it();
    the_keys_on_the_first_prompt_with_a_choice_only();
    a_choice_move_is_said_alone();
    a_press_at_the_end_says_nothing();
    a_message_says_its_text_and_the_key_every_time();
    a_message_says_nothing_of_the_choice();
    the_first_prompt_of_the_run_queues_and_later_ones_interrupt();
    a_new_visit_forgets_the_last_prompt();
    the_scene_left_with_a_prompt_unsaid_says_nothing();
    an_undecodable_text_is_left_out_and_logged_once();
    missing_choice_names_fall_back_to_the_forks();
    std::cout << "memcard_speech: all tests passed\n";
    return 0;
}
