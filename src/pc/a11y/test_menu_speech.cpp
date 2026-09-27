/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Menu speech against a fake screen reader bridge, fed snapshots of the main
 * menu tree written by hand. Links menu_speech.cpp, menu_names.cpp,
 * game_text.cpp and speech.cpp only, so this file supplies the pc_log_line
 * that main.c normally provides. */
#include "menu_names.hpp"
#include "menu_speech.hpp"
#include "speech.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<std::string> s_lines;

struct Output {
    std::string text;
    bool interrupt;

    bool operator==(const Output&) const = default;
};

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    explicit FakeBridge(std::vector<Output>& outputs) : m_outputs(outputs) {}

    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view utf8, bool interrupt) override {
        m_outputs.push_back(Output{std::string(utf8), interrupt});
        return "";
    }

private:
    std::vector<Output>& m_outputs;
};

/* Menu kinds and entries by the decomp's numbers (menu_kinds.h). */
constexpr int kMain = 0;
constexpr int k1P = 1;
constexpr int kVs = 2;
constexpr int kSettings = 4;
constexpr int kReg = 6;
constexpr int kRumble = 19;
constexpr int kSound = 20;
constexpr int kDisplay = 21;
constexpr int kMultiMan = 33;
constexpr int kOnline = 34;

/* Centre texts by their NTSC-U numbers, and their game text. */
constexpr int kChannel = 187;
constexpr int kVolume = 188;
constexpr int kDeflicker = 189;
constexpr int k10Man = 171;
constexpr int k100Man = 172;
constexpr int k3Minute = 173;
constexpr int kCruel = 176;
constexpr const char* kChannelText = "Choose between Stereo\nand Mono sound.";
constexpr const char* kVolumeText = "Adjust music and sound\neffect volume balance.";
constexpr const char* kDeflickerText =
    "ON: Display will be smoother and softer.\nOFF: Display will be sharper and harder.";

/* What a leaf screen with a reader shows: its centre text and the settings
 * it changes. Stereo, centred and deflickered unless a test says otherwise. */
struct Leaf {
    int center_text = -1;
    const char* text = nullptr;
    bool mono = false;
    int balance = 0;
    bool deflicker = true;
};

using Bytes = std::vector<std::uint8_t>;

/* NTSC-U game text for ASCII letters, digits, spaces and a few marks, a line
 * break where the text has '\n', and an unknown font glyph where it has '~'. */
Bytes game_text(const char* text) {
    Bytes out;
    auto glyph = [&out](unsigned code) {
        out.push_back(static_cast<std::uint8_t>(code >> 8));
        out.push_back(static_cast<std::uint8_t>(code));
    };
    for (; *text != '\0'; text++) {
        char c = *text;
        if (c == '\n') {
            out.push_back(0x03);
        } else if (c == ' ') {
            out.push_back(0x1A);
        } else if (c == '~') {
            glyph(0x4000);
        } else if (c >= '0' && c <= '9') {
            glyph(0x2000 + (c - '0'));
        } else if (c >= 'A' && c <= 'Z') {
            glyph(0x200A + (c - 'A'));
        } else if (c >= 'a' && c <= 'z') {
            glyph(0x2024 + (c - 'a'));
        } else if (c == '.') {
            glyph(0x20E7);
        } else if (c == ':') {
            glyph(0x20E9);
        } else if (c == '?') {
            glyph(0x20EB);
        } else {
            assert(c == '!');
            glyph(0x20EC);
        }
    }
    out.push_back(0x00);
    return out;
}

/* Speech over a fake bridge, and menu speech over that; s_lines only ever
 * holds the lines of the test that is running. */
struct Fixture {
    std::vector<Output> outputs;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(outputs)};
    a11y::MenuSpeech menu{speech, a11y::GameTextSource{}};

    Fixture() {
        speech.init();
        s_lines.clear();
    }

    /* One frame on a screen and entry; the entry's description is the game
     * text of description, if any. */
    void frame(int menu, int hovered, const char* description = nullptr) {
        Bytes bytes = description != nullptr ? game_text(description) : Bytes{};
        A11yMenuState state{};
        state.menu = menu;
        state.hovered = hovered;
        state.description = description != nullptr ? bytes.data() : nullptr;
        this->menu.frame(state);
    }

    void leaf_frame(int menu, const Leaf& leaf) {
        Bytes bytes = leaf.text != nullptr ? game_text(leaf.text) : Bytes{};
        A11yMenuState state{};
        state.menu = menu;
        state.center_text = leaf.center_text;
        state.center_text_string = leaf.text != nullptr ? bytes.data() : nullptr;
        state.mono = leaf.mono;
        state.balance = leaf.balance;
        state.deflicker = leaf.deflicker;
        this->menu.frame(state);
    }

    void online_frame(int menu, int hovered, const char* label, const char* description) {
        A11yMenuState state{};
        state.menu = menu;
        state.hovered = hovered;
        state.pc_label = label;
        state.pc_description = description;
        this->menu.frame(state);
    }

    /* What was spoken since the last call. */
    std::vector<std::string> spoken() {
        std::vector<std::string> texts;
        for (const Output& output : outputs) {
            assert(output.interrupt);
            texts.push_back(output.text);
        }
        outputs.clear();
        return texts;
    }
};

using Texts = std::vector<std::string>;

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

void arrival_on_the_main_menu() {
    Fixture f;
    f.frame(kMain, 0, "Solo Smash!");
    assert((f.spoken() == Texts{"Main Menu. 1-P Mode. Solo Smash!"}));
}

void arrival_on_a_landing_below_the_main_menu() {
    Fixture f;
    f.frame(kReg, 0, "Defeat each foe to advance.");
    assert((f.spoken() == Texts{"Regular Match. Classic. Defeat each foe to advance."}));
}

void unchanged_snapshot_speaks_nothing() {
    Fixture f;
    f.frame(kMain, 0, "Solo Smash!");
    f.spoken();
    f.frame(kMain, 0, "Solo Smash!");
    f.frame(kMain, 0, "Solo Smash!");
    assert(f.spoken().empty());
}

void cursor_moves_and_wraps() {
    Fixture f;
    f.frame(kMain, 3, "Adjust game settings.");
    f.spoken();
    f.frame(kMain, 4, "View data.");
    /* From the last entry to the first: nothing extra for the wrap. */
    f.frame(kMain, 0, "Solo Smash!");
    assert((f.spoken() == Texts{"Data. View data.", "1-P Mode. Solo Smash!"}));
}

void confirm_into_a_tree_screen_and_back() {
    Fixture f;
    f.frame(kMain, 0, "Solo Smash!");
    f.spoken();
    f.frame(k1P, 0, "Various events await.");
    f.frame(kMain, 0, "Solo Smash!");
    assert((f.spoken() == Texts{"1-P Mode. Regular Match. Various events await.",
                              "Main Menu. 1-P Mode. Solo Smash!"}));
}

void screen_change_and_cursor_move_in_one_step_is_the_screen() {
    Fixture f;
    f.frame(k1P, 3, "Test your skills.");
    f.spoken();
    f.frame(kMain, 1, "Multiplayer battles!");
    assert((f.spoken() == Texts{"Main Menu. VS. Mode. Multiplayer battles!"}));
}

void leaf_screen_says_its_name_and_the_return_speaks() {
    Fixture f;
    f.frame(kSettings, 0, "Turn rumble on or off.");
    f.spoken();
    f.frame(kRumble, 0);
    /* The leaf screen moves its own cursor: nothing is said. */
    f.frame(kRumble, 1);
    f.frame(kRumble, 2);
    f.frame(kSettings, 0, "Turn rumble on or off.");
    assert(
        (f.spoken() == Texts{"Rumble. No speech yet.", "Options. Rumble. Turn rumble on or off."}));
}

void forgetting_makes_the_same_state_an_arrival() {
    Fixture f;
    f.frame(kVs, 0, "Multiplayer battles!");
    f.spoken();
    /* Character select went back to the entry that started it. */
    f.menu.forget();
    f.frame(kVs, 0, "Multiplayer battles!");
    assert((f.spoken() == Texts{"VS. Mode. Melee. Multiplayer battles!"}));
}

void online_uses_the_base_port_strings() {
    Fixture f;
    f.online_frame(kVs, 5, "ONLINE", "Play against other players over the network.");
    f.online_frame(kOnline, 0, "LAN PLAY", "Play another player on your local network.");
    f.online_frame(kOnline, 1, "DIRECT CONNECT", "Connect to a friend using a connect code.");
    assert((f.spoken() == Texts{
                              "VS. Mode. Online. Play against other players over the network.",
                              "Online. LAN play. Play another player on your local network.",
                              "Direct connect. Connect to a friend using a connect code.",
                          }));
}

void missing_names_are_spoken_and_logged_once() {
    Fixture f;
    f.frame(127, 0);
    f.frame(k1P, 2, "Hidden.");
    f.frame(k1P, 3, "Test your skills.");
    f.frame(k1P, 2, "Hidden.");
    assert((f.spoken() == Texts{
                              "Unknown screen 127",
                              "1-P Mode. Unknown entry 2. Hidden.",
                              "Stadium. Test your skills.",
                              "Unknown entry 2. Hidden.",
                          }));
    assert(count_logged("[a11y] menu 127: not in the menu names table") == 1);
    assert(count_logged("[a11y] menu 1 entry 2: not in the menu names table") == 1);
}

void description_lines_are_joined() {
    Fixture f;
    f.frame(kMain, 4, "View data or change\nthe game settings.");
    assert((f.spoken() == Texts{"Main Menu. Data. View data or change the game settings."}));
}

void unknown_glyphs_are_left_out_and_logged() {
    Fixture f;
    f.frame(kMain, 2, "Pok~mon trophies!");
    assert((f.spoken() == Texts{"Main Menu. Trophies. Pokmon trophies!"}));
    assert(count_logged("[a11y] menu 0 entry 2: description \"Pok{glyph 4000}mon trophies!\", "
                        "unknown glyphs left out") == 1);
}

void missing_description_speaks_the_name() {
    Fixture f;
    f.frame(kMain, 1);
    assert((f.spoken() == Texts{"Main Menu. VS. Mode"}));
    assert(count_logged("[a11y] menu 0 entry 1: no description") == 1);
}

void sound_opens_on_the_channel_row() {
    Fixture f;
    f.frame(kSettings, 1, "Adjust sound settings.");
    f.spoken();
    f.leaf_frame(kSound, Leaf{kChannel, kChannelText});
    assert((f.spoken() == Texts{"Sound. Channel: Stereo. Left and right to change. Choose between "
                                "Stereo and Mono sound."}));
}

void sound_row_changes_both_ways() {
    Fixture f;
    f.leaf_frame(kSound, Leaf{kChannel, kChannelText});
    f.spoken();
    f.leaf_frame(kSound, Leaf{kVolume, kVolumeText});
    f.leaf_frame(kSound, Leaf{kChannel, kChannelText});
    assert((f.spoken() == Texts{"Volume: centre. Adjust music and sound effect volume balance.",
                              "Channel: Stereo. Choose between Stereo and Mono sound."}));
}

void sound_channel_changes_and_stays() {
    Fixture f;
    Leaf leaf{kChannel, kChannelText};
    f.leaf_frame(kSound, leaf);
    f.spoken();
    leaf.mono = true;
    f.leaf_frame(kSound, leaf);
    /* Right on Mono changes nothing. */
    f.leaf_frame(kSound, leaf);
    leaf.mono = false;
    f.leaf_frame(kSound, leaf);
    assert((f.spoken() == Texts{"Mono", "Stereo"}));
}

void sound_balance_says_the_distance_and_the_side() {
    Fixture f;
    Leaf leaf{kVolume, kVolumeText};
    f.leaf_frame(kSound, leaf);
    f.spoken();
    /* Repeats are presses at an end, which change nothing. */
    for (int balance : {5, 0, -5, -40, -100, -100, 95, 100, 100}) {
        leaf.balance = balance;
        f.leaf_frame(kSound, leaf);
    }
    assert((f.spoken() == Texts{"5 toward sounds", "Centre", "5 toward music", "40 toward music",
                              "100 toward music", "95 toward sounds", "100 toward sounds"}));
}

void sound_opens_with_the_balance_off_centre() {
    Fixture f;
    Leaf leaf{kVolume, kVolumeText};
    leaf.balance = -15;
    f.leaf_frame(kSound, leaf);
    assert((f.spoken() == Texts{"Sound. Volume: 15 toward music. Left and right to change. Adjust "
                                "music and sound effect volume balance."}));
}

void screen_display_turns_off_and_on() {
    Fixture f;
    Leaf leaf{kDeflicker, kDeflickerText};
    f.leaf_frame(kDisplay, leaf);
    leaf.deflicker = false;
    f.leaf_frame(kDisplay, leaf);
    leaf.deflicker = true;
    f.leaf_frame(kDisplay, leaf);
    assert((f.spoken() == Texts{"Screen Display. Deflicker: On. A to change. ON: Display will be "
                                "smoother and softer. OFF: Display will be sharper and harder.",
                              "Off", "On"}));
}

void multi_man_choices_change_and_wrap() {
    Fixture f;
    f.leaf_frame(kMultiMan, Leaf{k10Man, "How fast can you defeat 10 opponents?"});
    f.leaf_frame(kMultiMan, Leaf{k100Man, "100 enemies! Can you defeat them all?"});
    f.leaf_frame(kMultiMan, Leaf{kCruel, "Just try to survive!"});
    f.leaf_frame(kMultiMan, Leaf{k10Man, "How fast can you defeat 10 opponents?"});
    assert((f.spoken() == Texts{"Multi-Man Melee. 10-Man Melee. Left and right to choose. How fast "
                                "can you defeat 10 opponents?",
                              "100-Man Melee. 100 enemies! Can you defeat them all?",
                              "Cruel Melee. Just try to survive!",
                              "10-Man Melee. How fast can you defeat 10 opponents?"}));
}

void multi_man_opens_on_the_mode_just_played() {
    Fixture f;
    /* The match ended; the menu scene opens straight on Multi-Man Melee. */
    f.menu.forget();
    f.leaf_frame(kMultiMan, Leaf{k3Minute, "How many can you defeat in 3 minutes?"});
    assert((f.spoken() == Texts{"Multi-Man Melee. 3-Minute Melee. Left and right to choose. How "
                                "many can you defeat in 3 minutes?"}));
}

void reader_screen_leaves_to_the_tree() {
    Fixture f;
    f.leaf_frame(kDisplay, Leaf{kDeflicker, kDeflickerText});
    f.spoken();
    f.frame(kSettings, 2, "Adjust screen settings.");
    assert((f.spoken() == Texts{"Options. Screen Display. Adjust screen settings."}));
}

void unknown_centre_text_is_spoken_and_logged() {
    Fixture f;
    f.leaf_frame(kSound, Leaf{191, nullptr});
    assert((f.spoken() == Texts{"Sound. Unknown row 191. Left and right to change."}));
    assert(count_logged("[a11y] menu 20 centre text 191: not in the menu names table") == 1);
    assert(count_logged("[a11y] menu 20 centre text 191: no description") == 1);
}

void plain_capitals_keeps_abbreviations() {
    assert(a11y::plain_capitals("DIRECT CONNECT") == "Direct connect");
    assert(a11y::plain_capitals("LAN PLAY") == "LAN play");
    assert(a11y::plain_capitals("RANKED") == "Ranked");
    assert(a11y::plain_capitals("Already Mixed") == "Already Mixed");
    assert(a11y::plain_capitals("") == "");
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
    arrival_on_the_main_menu();
    arrival_on_a_landing_below_the_main_menu();
    unchanged_snapshot_speaks_nothing();
    cursor_moves_and_wraps();
    confirm_into_a_tree_screen_and_back();
    screen_change_and_cursor_move_in_one_step_is_the_screen();
    leaf_screen_says_its_name_and_the_return_speaks();
    forgetting_makes_the_same_state_an_arrival();
    online_uses_the_base_port_strings();
    missing_names_are_spoken_and_logged_once();
    description_lines_are_joined();
    unknown_glyphs_are_left_out_and_logged();
    missing_description_speaks_the_name();
    sound_opens_on_the_channel_row();
    sound_row_changes_both_ways();
    sound_channel_changes_and_stays();
    sound_balance_says_the_distance_and_the_side();
    sound_opens_with_the_balance_off_centre();
    screen_display_turns_off_and_on();
    multi_man_choices_change_and_wrap();
    multi_man_opens_on_the_mode_just_played();
    reader_screen_leaves_to_the_tree();
    unknown_centre_text_is_spoken_and_logged();
    plain_capitals_keeps_abbreviations();
    std::cout << "menu_speech: all tests passed\n";
    return 0;
}
