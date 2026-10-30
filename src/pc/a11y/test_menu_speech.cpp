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
constexpr int kRules = 13;
constexpr int kAdditionalRules = 15;
constexpr int kItemSwitch = 16;
constexpr int kRandomStage = 17;
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

/* The rules screens' descriptions as the game's text has them, by row, and
 * by value where the row has one per value. */
constexpr const char* kTimeText = "Compete for points based on KOs\nand Falls in a timed battle.";
constexpr const char* kStockModeText = "Battle to be the last one standing.";
constexpr const char* kCoinText = "Battle to collect the most Coins.";
constexpr const char* kTimeLimitText =
    "Set the time limit from 1 to 99\nminutes, turn the time limit OFF.";
constexpr const char* kStockCountText = "Set the number of stock lives.";
constexpr const char* kHandicapOffText = "Play without handicaps.";
constexpr const char* kHandicapAutoText =
    "Handicaps are set automatically\nbased on win percentages.";
constexpr const char* kDamageRatioText = "The larger the damage ratio,\nthe faster you get KO'd.";
constexpr const char* kStageOnText = "Choose stages to battle on.";
constexpr const char* kStageTurnsText = "Players take turns choosing stages.";
constexpr const char* kItemSwitchText =
    "Choose how often items will appear\nand turn specific items on or off.";
constexpr const char* kAdditionalRulesText = "Set even more detailed rules.";
constexpr const char* kStockTimeLimitText = "Set a time limit for Stock Matches.";
constexpr const char* kFriendlyFireOffText = "Team members cannot damage\neach other.";
constexpr const char* kPauseOnText = "Players will be able to pause\nin mid-game.";
constexpr const char* kScoreOffText = "Score will not be displayed on screen.";
constexpr const char* kSelfDestructsMinus1Text =
    "Self-destructs will subtract one\npoint from players' scores.";
constexpr const char* kSelfDestructs0Text = "Self-destructs will not affect\nplayers' scores.";
constexpr const char* kSelfDestructsMinus2Text =
    "Self-destructs will subtract two\npoints from players' scores.";
constexpr const char* kRandomStageText =
    "If stage selection is set at Random, players\nwill be able to choose stages to exclude.";

/* Item Switch and Random Stage's key hint. */
constexpr const char* kSwitchHint =
    "Up and down to move, left and right to change column, A to turn on or off.";
/* Item Switch's rows: the columns' first and last items, and the frequency
 * strip above them, entered from the left column or the right. */
constexpr int kFood = 0;
constexpr int kMaximTomato = 1;
constexpr int kFlipper = 15;
constexpr int kFreezie = 16;
constexpr int kMrSaturn = 17;
constexpr int kPartyBall = 30;
constexpr int kStripLeft = 31;
constexpr int kStripRight = 32;
constexpr int kMedium = 3;

/* The rules as a new save has them: a 2-minute time match. */
A11yRules default_rules() {
    A11yRules rules{};
    rules.mode = 0;
    rules.time_limit = 2;
    rules.stock_count = 3;
    rules.handicap = 0;
    rules.damage_ratio = 10;
    rules.stage_selection = 0;
    rules.stock_time_limit = 8;
    rules.friendly_fire = false;
    rules.pause = true;
    rules.score_display = false;
    rules.self_destructs = 0;
    return rules;
}

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
 * break where the text has '\n', SdMenu's font glyphs 0x4000 (é) where it
 * has '@' and 0x4002 (II) where it has '#', and 0x4001, unknown, where it
 * has '~'. */
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
        } else if (c == '@') {
            glyph(0x4000);
        } else if (c == '~') {
            glyph(0x4001);
        } else if (c == '#') {
            glyph(0x4002);
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
        } else if (c == ',') {
            glyph(0x20E6);
        } else if (c == '\'') {
            glyph(0x20F3);
        } else if (c == '-') {
            glyph(0x20FC);
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

    /* One frame on a rules screen: the row, the rules, and the row's
     * description as the screen shows it for the row's value. */
    void rules_frame(int menu, int row, const A11yRules& rules, const char* description) {
        Bytes bytes = description != nullptr ? game_text(description) : Bytes{};
        A11yMenuState state{};
        state.menu = menu;
        state.hovered = row;
        state.description = description != nullptr ? bytes.data() : nullptr;
        state.rules = rules;
        this->menu.frame(state);
    }

    /* One frame on Item Switch or Random Stage: the row and its value as
     * the screen's input set it, and whether a toggle was refused since the
     * last frame. */
    void switch_frame(int menu, int row, int value, bool refused = false) {
        A11yMenuState state{};
        state.menu = menu;
        state.hovered = row;
        state.confirmed = value;
        state.refused = refused;
        state.rules = default_rules();
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
    assert(count_logged("[a11y] menu 0 entry 2: description \"Pok{glyph 4001}mon trophies!\", "
                        "unknown glyphs left out") == 1);
}

/* The menu font's own glyphs, as SdMenu and SdSlChr hold them. */
void the_menu_fonts_glyphs_are_said() {
    Fixture f;
    f.frame(kMain, 2, "Pok@mon and Mushroom Kingdom #!");
    assert((f.spoken() == Texts{"Main Menu. Trophies. Pokémon and Mushroom Kingdom II!"}));
    assert(s_lines.size() == 1);
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

void custom_rules_opens_from_vs_mode() {
    Fixture f;
    f.frame(kVs, 3, "Set the rules for VS. matches.");
    f.spoken();
    f.rules_frame(kRules, 0, default_rules(), kTimeText);
    assert((f.spoken() == Texts{"Custom Rules. Rules: Time. Left and right to change. Compete for "
                                "points based on KOs and Falls in a timed battle."}));
}

void additional_rules_opens_from_custom_rules() {
    Fixture f;
    f.rules_frame(kRules, 6, default_rules(), kAdditionalRulesText);
    f.spoken();
    A11yRules rules = default_rules();
    rules.stock_time_limit = 0;
    f.rules_frame(kAdditionalRules, 0, rules, kStockTimeLimitText);
    assert((f.spoken() == Texts{"Additional Rules. Stock match time limit: none. Left and right to "
                                "change. Set a time limit for Stock Matches."}));
}

void custom_rules_rows_down_and_wrap() {
    Fixture f;
    A11yRules rules = default_rules();
    f.rules_frame(kRules, 0, rules, kTimeText);
    f.spoken();
    f.rules_frame(kRules, 1, rules, kTimeLimitText);
    f.rules_frame(kRules, 2, rules, kHandicapOffText);
    f.rules_frame(kRules, 3, rules, kDamageRatioText);
    f.rules_frame(kRules, 4, rules, kStageOnText);
    f.rules_frame(kRules, 5, rules, kItemSwitchText);
    f.rules_frame(kRules, 6, rules, kAdditionalRulesText);
    /* Down on the last row goes to the first: nothing extra for the wrap. */
    f.rules_frame(kRules, 0, rules, kTimeText);
    /* And Up from the first to the last. */
    f.rules_frame(kRules, 6, rules, kAdditionalRulesText);
    assert((
        f.spoken() ==
        Texts{
            "Time limit: 2 minutes. Set the time limit from 1 to 99 minutes, turn the time "
            "limit OFF.",
            "Handicap: Off. Play without handicaps.",
            "Damage ratio: 1.0. The larger the damage ratio, the faster you get KO'd.",
            "Stage selection: On. Choose stages to battle on.",
            "Item Switch. Choose how often items will appear and turn specific items on or off.",
            "Additional Rules. Set even more detailed rules.",
            "Rules: Time. Compete for points based on KOs and Falls in a timed battle.",
            "Additional Rules. Set even more detailed rules.",
        }));
}

void rules_change_to_stock_renames_the_next_row() {
    Fixture f;
    A11yRules rules = default_rules();
    f.rules_frame(kRules, 0, rules, kTimeText);
    f.spoken();
    rules.mode = 1;
    f.rules_frame(kRules, 0, rules, kStockModeText);
    f.rules_frame(kRules, 1, rules, kStockCountText);
    rules.stock_count = 4;
    f.rules_frame(kRules, 1, rules, kStockCountText);
    f.rules_frame(kRules, 0, rules, kStockModeText);
    rules.mode = 2;
    f.rules_frame(kRules, 0, rules, kCoinText);
    f.rules_frame(kRules, 1, rules, kTimeLimitText);
    assert((f.spoken() == Texts{
                              "Stock. Battle to be the last one standing.",
                              "Stock: 3. Set the number of stock lives.",
                              "4",
                              "Rules: Stock. Battle to be the last one standing.",
                              "Coin. Battle to collect the most Coins.",
                              "Time limit: 2 minutes. Set the time limit from 1 to 99 minutes, "
                              "turn the time limit OFF.",
                          }));
}

void value_change_with_and_without_its_description() {
    Fixture f;
    A11yRules rules = default_rules();
    f.rules_frame(kRules, 2, rules, kHandicapOffText);
    f.spoken();
    rules.handicap = 1;
    f.rules_frame(kRules, 2, rules, kHandicapAutoText);
    /* An unchanged frame says nothing. */
    f.rules_frame(kRules, 2, rules, kHandicapAutoText);
    f.rules_frame(kRules, 4, rules, kStageOnText);
    rules.stage_selection = 3;
    f.rules_frame(kRules, 4, rules, kStageTurnsText);
    f.rules_frame(kRules, 3, rules, kDamageRatioText);
    rules.damage_ratio = 12;
    f.rules_frame(kRules, 3, rules, kDamageRatioText);
    assert((f.spoken() ==
            Texts{
                "Auto. Handicaps are set automatically based on win percentages.",
                "Stage selection: On. Choose stages to battle on.",
                "Turns. Players take turns choosing stages.",
                "Damage ratio: 1.0. The larger the damage ratio, the faster you get KO'd.",
                "1.2",
            }));
}

void time_limit_none_one_minute_and_99() {
    Fixture f;
    A11yRules rules = default_rules();
    f.rules_frame(kRules, 1, rules, kTimeLimitText);
    f.spoken();
    /* Left from 2 to none, then on round to 99 and back. */
    for (int minutes : {1, 0, 99, 0, 1}) {
        rules.time_limit = minutes;
        f.rules_frame(kRules, 1, rules, kTimeLimitText);
    }
    assert((f.spoken() == Texts{"1 minute", "None", "99 minutes", "None", "1 minute"}));
}

void damage_ratio_at_both_ends() {
    Fixture f;
    A11yRules rules = default_rules();
    rules.damage_ratio = 5;
    f.rules_frame(kRules, 3, rules, kDamageRatioText);
    rules.damage_ratio = 20;
    f.rules_frame(kRules, 3, rules, kDamageRatioText);
    rules.damage_ratio = 5;
    f.rules_frame(kRules, 3, rules, kDamageRatioText);
    assert((f.spoken() == Texts{"Custom Rules. Damage ratio: 0.5. Left and right to change. The "
                                "larger the damage ratio, the faster you get KO'd.",
                              "2.0", "0.5"}));
}

void self_destruct_values() {
    Fixture f;
    A11yRules rules = default_rules();
    f.rules_frame(kAdditionalRules, 2, rules, kPauseOnText);
    f.spoken();
    f.rules_frame(kAdditionalRules, 4, rules, kSelfDestructsMinus1Text);
    rules.self_destructs = 1;
    f.rules_frame(kAdditionalRules, 4, rules, kSelfDestructs0Text);
    rules.self_destructs = 2;
    f.rules_frame(kAdditionalRules, 4, rules, kSelfDestructsMinus2Text);
    rules.self_destructs = 0;
    f.rules_frame(kAdditionalRules, 4, rules, kSelfDestructsMinus1Text);
    assert((f.spoken() ==
            Texts{
                /* Pause to Self-destructs: Score Display is hidden, and the
                 * cursor skips it. */
                "Self-destructs: minus 1. Self-destructs will subtract one point from players' "
                "scores.",
                "0. Self-destructs will not affect players' scores.",
                "Minus 2. Self-destructs will subtract two points from players' scores.",
                "Minus 1. Self-destructs will subtract one point from players' scores.",
            }));
}

void additional_rules_rows_and_wrap() {
    Fixture f;
    A11yRules rules = default_rules();
    rules.score_display = false;
    f.rules_frame(kAdditionalRules, 0, rules, kStockTimeLimitText);
    f.spoken();
    f.rules_frame(kAdditionalRules, 1, rules, kFriendlyFireOffText);
    f.rules_frame(kAdditionalRules, 2, rules, kPauseOnText);
    /* Score Display shown, once it is unlocked. */
    f.rules_frame(kAdditionalRules, 3, rules, kScoreOffText);
    f.rules_frame(kAdditionalRules, 4, rules, kSelfDestructsMinus1Text);
    f.rules_frame(kAdditionalRules, 5, rules, kRandomStageText);
    f.rules_frame(kAdditionalRules, 0, rules, kStockTimeLimitText);
    assert((f.spoken() ==
            Texts{
                "Friendly fire: Off. Team members cannot damage each other.",
                "Pause: On. Players will be able to pause in mid-game.",
                "Score display: Off. Score will not be displayed on screen.",
                "Self-destructs: minus 1. Self-destructs will subtract one point from players' "
                "scores.",
                "Random Stage. If stage selection is set at Random, players will be able to "
                "choose stages to exclude.",
                "Stock match time limit: 8 minutes. Set a time limit for Stock Matches.",
            }));
}

void left_and_right_on_a_row_that_opens_a_screen_say_nothing() {
    Fixture f;
    A11yRules rules = default_rules();
    f.rules_frame(kRules, 5, rules, kItemSwitchText);
    f.spoken();
    f.rules_frame(kRules, 5, rules, kItemSwitchText);
    assert(f.spoken().empty());
}

void returns_from_the_screens_rows_open() {
    Fixture f;
    A11yRules rules = default_rules();
    f.rules_frame(kRules, 5, rules, kItemSwitchText);
    f.spoken();
    f.switch_frame(kItemSwitch, 0, 1);
    f.rules_frame(kRules, 5, rules, kItemSwitchText);
    f.rules_frame(kRules, 6, rules, kAdditionalRulesText);
    f.rules_frame(kAdditionalRules, 0, rules, kStockTimeLimitText);
    f.rules_frame(kAdditionalRules, 5, rules, kRandomStageText);
    f.switch_frame(kRandomStage, 0, 1);
    f.rules_frame(kAdditionalRules, 5, rules, kRandomStageText);
    f.rules_frame(kRules, 6, rules, kAdditionalRulesText);
    f.frame(kVs, 3, "Set the rules for VS. matches.");
    assert((f.spoken() ==
            Texts{
                std::string("Item Switch. Food: on. ") + kSwitchHint,
                "Custom Rules. Item Switch. Choose how often items will appear and turn specific "
                "items on or off.",
                "Additional Rules. Set even more detailed rules.",
                "Additional Rules. Stock match time limit: 8 minutes. Left and right to change. "
                "Set a time limit for Stock Matches.",
                "Random Stage. If stage selection is set at Random, players will be able to "
                "choose stages to exclude.",
                std::string("Random Stage. Princess Peach's Castle: on. ") + kSwitchHint,
                "Additional Rules. Random Stage. If stage selection is set at Random, players "
                "will be able to choose stages to exclude.",
                "Custom Rules. Additional Rules. Set even more detailed rules.",
                "VS. Mode. Custom Rules. Set the rules for VS. matches.",
            }));
}

void a_forgotten_return_is_an_opening() {
    Fixture f;
    A11yRules rules = default_rules();
    f.frame(kItemSwitch, 0);
    f.spoken();
    f.menu.forget();
    f.rules_frame(kRules, 5, rules, kItemSwitchText);
    assert((f.spoken() == Texts{"Custom Rules. Item Switch. Left and right to change. Choose how "
                                "often items will appear and turn specific items on or off."}));
}

void unknown_rules_row_and_value_are_spoken_and_logged() {
    Fixture f;
    A11yRules rules = default_rules();
    f.rules_frame(kRules, 0, rules, kTimeText);
    f.spoken();
    f.rules_frame(kRules, 9, rules, nullptr);
    rules.handicap = 7;
    f.rules_frame(kRules, 2, rules, nullptr);
    assert((f.spoken() == Texts{"Unknown row 9", "Handicap: 7"}));
    assert(count_logged("[a11y] menu 13 row 9: not in the menu names table") == 1);
    assert(count_logged("[a11y] menu 13 row 9: no description") == 1);
    assert(count_logged("[a11y] menu 13 row 2: value 7 not in the menu names table") == 1);
}

void item_switch_opens_from_custom_rules() {
    Fixture f;
    f.rules_frame(kRules, 5, default_rules(), kItemSwitchText);
    f.spoken();
    f.switch_frame(kItemSwitch, kFood, 1);
    assert((f.spoken() == Texts{std::string("Item Switch. Food: on. ") + kSwitchHint}));
}

void item_switch_moves_within_and_between_columns() {
    Fixture f;
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    f.switch_frame(kItemSwitch, kMaximTomato, 1);
    /* Right: the other column at the same row. */
    f.switch_frame(kItemSwitch, kMrSaturn, 0);
    f.switch_frame(kItemSwitch, kFreezie, 1);
    /* Left: back. */
    f.switch_frame(kItemSwitch, kFood, 1);
    f.switch_frame(kItemSwitch, kFlipper, 1);
    /* Right from Flipper clamps to Party Ball. */
    f.switch_frame(kItemSwitch, kPartyBall, 0);
    assert((f.spoken() == Texts{
                              "Maxim Tomato: on",
                              "Mr. Saturn: off",
                              "Freezie: on",
                              "Food: on",
                              "Flipper: on",
                              "Party Ball: off",
                          }));
}

void item_names_are_the_columns_as_drawn() {
    const char* names[] = {"Food", "Maxim Tomato", "Heart Container", "Warp Star", "Ray Gun",
        "Super Scope", "Fire Flower", "Lip's Stick", "Star Rod", "Beam Sword", "Home-Run Bat",
        "Fan", "Hammer", "Green Shell", "Red Shell", "Flipper", "Freezie", "Mr. Saturn",
        "Poké Ball", "Bob-omb", "Motion-Sensor Bomb", "Super Mushroom", "Poison Mushroom",
        "Starman", "Parasol", "Screw Attack", "Metal Box", "Bunny Hood", "Cloaking Device",
        "Barrel Cannon", "Party Ball"};
    Fixture f;
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    for (int row = 1; row <= kPartyBall; row++) {
        f.switch_frame(kItemSwitch, row, 1);
        assert((f.spoken() == Texts{std::string(names[row]) + ": on"}));
    }
}

void item_frequency_strip_wraps_with_its_hint_once() {
    Fixture f;
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    /* Up from Food wraps to the strip, Up again to the column's foot. */
    f.switch_frame(kItemSwitch, kStripLeft, kMedium);
    f.switch_frame(kItemSwitch, kFlipper, 1);
    f.switch_frame(kItemSwitch, kStripLeft, kMedium);
    f.switch_frame(kItemSwitch, kFood, 1);
    /* Right column: Down from Party Ball to the strip, then to Freezie. */
    f.switch_frame(kItemSwitch, kFreezie, 1);
    f.switch_frame(kItemSwitch, kPartyBall, 1);
    f.switch_frame(kItemSwitch, kStripRight, kMedium);
    f.switch_frame(kItemSwitch, kFreezie, 1);
    assert((f.spoken() == Texts{
                              "Item frequency: Medium. Left and right to adjust.",
                              "Flipper: on",
                              "Item frequency: Medium",
                              "Food: on",
                              "Freezie: on",
                              "Party Ball: on",
                              "Item frequency: Medium",
                              "Freezie: on",
                          }));
}

void item_frequency_hint_again_after_reopening() {
    Fixture f;
    A11yRules rules = default_rules();
    f.switch_frame(kItemSwitch, kFood, 1);
    f.switch_frame(kItemSwitch, kStripLeft, kMedium);
    f.rules_frame(kRules, 5, rules, kItemSwitchText);
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    f.switch_frame(kItemSwitch, kStripLeft, kMedium);
    assert((f.spoken() == Texts{"Item frequency: Medium. Left and right to adjust."}));
    /* A scene entered (character select's rules screens) reopens it too. */
    f.menu.forget();
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    f.switch_frame(kItemSwitch, kStripRight, kMedium);
    assert((f.spoken() == Texts{"Item frequency: Medium. Left and right to adjust."}));
}

void item_frequency_says_every_word() {
    Fixture f;
    f.switch_frame(kItemSwitch, kFood, 1);
    f.switch_frame(kItemSwitch, kStripLeft, kMedium);
    f.spoken();
    /* Left is more, wrapping from Very High to None. */
    for (int frequency : {4, 5, 0, 1, 2, 3}) {
        f.switch_frame(kItemSwitch, kStripLeft, frequency);
    }
    assert((f.spoken() == Texts{"High", "Very high", "None", "Very low", "Low", "Medium"}));
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    f.switch_frame(kItemSwitch, kStripLeft, 0);
    assert((f.spoken() == Texts{"Item frequency: None"}));
}

void item_toggles_both_ways() {
    Fixture f;
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    f.switch_frame(kItemSwitch, kFood, 0);
    f.switch_frame(kItemSwitch, kFood, 1);
    assert((f.spoken() == Texts{"Off", "On"}));
}

void a_switch_press_that_changes_nothing_says_nothing() {
    Fixture f;
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    /* Left on the left column; A on the strip. */
    f.switch_frame(kItemSwitch, kFood, 1);
    f.switch_frame(kItemSwitch, kStripLeft, kMedium);
    f.spoken();
    f.switch_frame(kItemSwitch, kStripLeft, kMedium);
    /* Item Switch refuses nothing; a stray report says nothing. */
    f.switch_frame(kItemSwitch, kStripLeft, kMedium, true);
    assert(f.spoken().empty());
}

void unknown_item_frequency_is_spoken_and_logged() {
    Fixture f;
    f.switch_frame(kItemSwitch, kFood, 1);
    f.spoken();
    f.switch_frame(kItemSwitch, kStripLeft, 9);
    assert((f.spoken() == Texts{"Item frequency: 9. Left and right to adjust."}));
    assert(count_logged("[a11y] menu 16 row 31: value 9 not in the menu names table") == 1);
    /* The switch screens have no description line, so none is missed. */
    assert(count_logged("[a11y] menu 16 row 31: no description") == 0);
    assert(count_logged("[a11y] menu 16 row 0: no description") == 0);
}

void random_stage_opens_from_additional_rules() {
    Fixture f;
    f.rules_frame(kAdditionalRules, 5, default_rules(), kRandomStageText);
    f.spoken();
    f.switch_frame(kRandomStage, 0, 1);
    assert((f.spoken() ==
            Texts{std::string("Random Stage. Princess Peach's Castle: on. ") + kSwitchHint}));
}

void random_stage_names_every_row() {
    const char* names[] = {"Princess Peach's Castle", "Kongo Jungle", "Great Bay", "Brinstar",
        "Yoshi's Story", "Fountain of Dreams", "Corneria", "Pokémon Stadium", "Mute City", "Onett",
        "Mushroom Kingdom", "Icicle Mountain", "Battlefield", "Dream Land N64", "Kongo Jungle N64",
        "Rainbow Cruise", "Jungle Japes", "Hyrule Temple", "Brinstar Depths", "Yoshi's Island",
        "Green Greens", "Venom", "Poké Floats", "Big Blue", "Fourside", "Mushroom Kingdom II",
        "Flat Zone", "Final Destination", "Yoshi's Island N64"};
    Fixture f;
    f.switch_frame(kRandomStage, 0, 1);
    f.spoken();
    for (int row = 1; row < 29; row++) {
        f.switch_frame(kRandomStage, row, row % 2);
        assert((f.spoken() == Texts{std::string(names[row]) + (row % 2 != 0 ? ": on" : ": off")}));
    }
}

void random_stage_toggles_and_refuses() {
    constexpr int kCorneria = 6;
    Fixture f;
    f.switch_frame(kRandomStage, 0, 1);
    f.switch_frame(kRandomStage, kCorneria, 1);
    f.spoken();
    f.switch_frame(kRandomStage, kCorneria, 0);
    f.switch_frame(kRandomStage, kCorneria, 1);
    /* Turning off the last stage that is on: the game keeps it on. */
    f.switch_frame(kRandomStage, kCorneria, 1, true);
    /* A press that changes nothing, as Left on the left column. */
    f.switch_frame(kRandomStage, kCorneria, 1);
    f.switch_frame(kRandomStage, kCorneria, 1, true);
    assert((f.spoken() == Texts{"Off", "On", "At least one stage must stay on.",
                              "At least one stage must stay on."}));
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
    the_menu_fonts_glyphs_are_said();
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
    custom_rules_opens_from_vs_mode();
    additional_rules_opens_from_custom_rules();
    custom_rules_rows_down_and_wrap();
    rules_change_to_stock_renames_the_next_row();
    value_change_with_and_without_its_description();
    time_limit_none_one_minute_and_99();
    damage_ratio_at_both_ends();
    self_destruct_values();
    additional_rules_rows_and_wrap();
    left_and_right_on_a_row_that_opens_a_screen_say_nothing();
    returns_from_the_screens_rows_open();
    a_forgotten_return_is_an_opening();
    unknown_rules_row_and_value_are_spoken_and_logged();
    item_switch_opens_from_custom_rules();
    item_switch_moves_within_and_between_columns();
    item_names_are_the_columns_as_drawn();
    item_frequency_strip_wraps_with_its_hint_once();
    item_frequency_hint_again_after_reopening();
    item_frequency_says_every_word();
    item_toggles_both_ways();
    a_switch_press_that_changes_nothing_says_nothing();
    unknown_item_frequency_is_spoken_and_logged();
    random_stage_opens_from_additional_rules();
    random_stage_names_every_row();
    random_stage_toggles_and_refuses();
    plain_capitals_keeps_abbreviations();
    std::cout << "menu_speech: all tests passed\n";
    return 0;
}
