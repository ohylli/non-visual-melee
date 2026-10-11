/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_names.hpp"
#include "menu_kinds.h"
#include "sss_names.hpp"
#include <iterator>
#include <span>

namespace a11y {
namespace {

enum class MenuKind : int {
#define A11Y_MENU_KIND_ENUMERATOR(name, number) name = number,
    A11Y_MENU_KINDS(A11Y_MENU_KIND_ENUMERATOR)
#undef A11Y_MENU_KIND_ENUMERATOR
};

enum class RulesMode : int {
#define A11Y_RULES_MODE_ENUMERATOR(name, number) name = number,
    A11Y_RULES_MODES(A11Y_RULES_MODE_ENUMERATOR)
#undef A11Y_RULES_MODE_ENUMERATOR
};

enum class StageSelectMode : int {
#define A11Y_STAGE_SELECT_MODE_ENUMERATOR(name, number) name = number,
    A11Y_STAGE_SELECT_MODES(A11Y_STAGE_SELECT_MODE_ENUMERATOR)
#undef A11Y_STAGE_SELECT_MODE_ENUMERATOR
};

constexpr int kind(MenuKind menu) {
    return static_cast<int>(menu);
}

/* Unscoped: each tree screen has its own enum in the decomp, and the names do
 * not collide. */
enum MenuSelection : int {
#define A11Y_MENU_SELECTION_ENUMERATOR(name, number) name = number,
    A11Y_MENU_SELECTIONS(A11Y_MENU_SELECTION_ENUMERATOR)
#undef A11Y_MENU_SELECTION_ENUMERATOR
};

struct EntryName {
    MenuSelection entry;
    std::string_view name;
};

struct ScreenName {
    MenuKind menu;
    std::string_view name;
    bool tree;
    std::span<const EntryName> entries;
};

/* Entries in cursor order. All-Star and Sound Test are absent until
 * unlocked; the cursor skips them then. */
constexpr EntryName kMainEntries[] = {
    {SEL_MAIN_1P, "1-P Mode"},
    {SEL_MAIN_VS, "VS. Mode"},
    {SEL_MAIN_TOY, "Trophies"},
    {SEL_MAIN_SETTINGS, "Options"},
    {SEL_MAIN_DATA, "Data"},
};
constexpr EntryName k1PEntries[] = {
    {SEL_1P_REG, "Regular Match"},
    {SEL_1P_EVENT, "Event Match"},
    {SEL_1P_STADIUM, "Stadium"},
    {SEL_1P_TRAINING, "Training"},
};
constexpr EntryName kRegularEntries[] = {
    {SEL_REG_CLASSIC, "Classic"},
    {SEL_REG_ADVENTURE, "Adventure"},
    {SEL_REG_ALLSTAR, "All-Star"},
};
constexpr EntryName kStadiumEntries[] = {
    {SEL_STADIUM_TARGET, "Target Test"},
    {SEL_STADIUM_HOMERUN, "Home-Run Contest"},
    {SEL_STADIUM_MULTIMAN, "Multi-Man Melee"},
};
/* The Online entry is named by the base port at runtime; this is the
 * fallback. */
constexpr EntryName kVsEntries[] = {
    {SEL_VS_MELEE, "Melee"},
    {SEL_VS_TOURNAMENT, "Tournament Melee"},
    {SEL_VS_SPECIAL, "Special Melee"},
    {SEL_VS_RULES, "Custom Rules"},
    {SEL_VS_NAME, "Name Entry"},
    {SEL_VS_ONLINE, "Online"},
};
constexpr EntryName kSpecialEntries[] = {
    {SEL_SPECIAL_VS_CAMERA, "Camera Mode"},
    {SEL_SPECIAL_VS_STAMINA, "Stamina Mode"},
    {SEL_SPECIAL_VS_SUDDEN_DEATH, "Super Sudden Death"},
    {SEL_SPECIAL_VS_GIANT, "Giant Melee"},
    {SEL_SPECIAL_VS_TINY, "Tiny Melee"},
    {SEL_SPECIAL_VS_INVISIBLE, "Invisible Melee"},
    {SEL_SPECIAL_VS_FIXED_CAMERA, "Fixed-Camera Mode"},
    {SEL_SPECIAL_VS_SINGLE_BUTTON, "Single-Button Mode"},
    {SEL_SPECIAL_VS_LIGHTNING, "Lightning Melee"},
    {SEL_SPECIAL_VS_SLOMO, "Slo-Mo Melee"},
};
constexpr EntryName kTrophyEntries[] = {
    {SEL_TOY_GALLERY, "Gallery"},
    {SEL_TOY_LOTTERY, "Lottery"},
    {SEL_TOY_COLLECTION, "Collection"},
};
/* The Language entry is drawn in Japanese while the game is in English. */
constexpr EntryName kOptionsEntries[] = {
    {SEL_SETTINGS_RUMBLE, "Rumble"},
    {SEL_SETTINGS_SOUND, "Sound"},
    {SEL_SETTINGS_DISPLAY, "Screen Display"},
    {SEL_SETTINGS_LANG, "Language"},
    {SEL_SETTINGS_ERASE, "Erase Data"},
};
constexpr EntryName kDataEntries[] = {
    {SEL_DATA_SNAP, "Snapshots"},
    {SEL_DATA_ARCHIVES, "Archives"},
    {SEL_DATA_SOUND, "Sound Test"},
    {SEL_DATA_RECORDS, "Melee Records"},
    {SEL_DATA_SPECIAL, "Special"},
};
constexpr EntryName kRecordsEntries[] = {
    {SEL_RECORDS_VS, "VS. Records"},
    {SEL_RECORDS_BONUS, "Bonus Records"},
    {SEL_RECORDS_MISC, "Misc. Records"},
};

/* A leaf screen's name is its title as drawn, or what the screen calls itself
 * where the title tab shows its parent. Checked against screenshots of an
 * NTSC-U disc; Language is drawn in Japanese and spoken in English. */
constexpr ScreenName kScreens[] = {
    /* Tree screens. */
    {MenuKind::MENU_KIND_MAIN, "Main Menu", true, kMainEntries},
    {MenuKind::MENU_KIND_1P, "1-P Mode", true, k1PEntries},
    {MenuKind::MENU_KIND_REG, "Regular Match", true, kRegularEntries},
    {MenuKind::MENU_KIND_STADIUM, "Stadium", true, kStadiumEntries},
    {MenuKind::MENU_KIND_VS, "VS. Mode", true, kVsEntries},
    {MenuKind::MENU_KIND_SPECIAL, "Special Melee", true, kSpecialEntries},
    {MenuKind::MENU_KIND_TOY, "Trophies", true, kTrophyEntries},
    {MenuKind::MENU_KIND_SETTINGS, "Options", true, kOptionsEntries},
    {MenuKind::MENU_KIND_DATA, "Data", true, kDataEntries},
    {MenuKind::MENU_KIND_RECORDS, "Melee Records", true, kRecordsEntries},
    /* The screen keeps the VS. Mode title picture. */
    {MenuKind::MENU_KIND_ONLINE, "Online", true, {}},

    /* Leaf screens. */
    {MenuKind::MENU_KIND_EVENT, "Event Match", false, {}},
    {MenuKind::MENU_KIND_MULTI_VS, "Multi-Man Melee", false, {}},
    {MenuKind::MENU_KIND_RULES, "Custom Rules", false, {}},
    {MenuKind::MENU_KIND_RULES_EXTRA, "Additional Rules", false, {}},
    {MenuKind::MENU_KIND_RULES_ITEMS, "Item Switch", false, {}},
    {MenuKind::MENU_KIND_RULES_STAGE, "Random Stage", false, {}},
    /* The name list and the keyboard both. */
    {MenuKind::MENU_KIND_NAME_ENTRY, "Name Entry", false, {}},
    {MenuKind::MENU_KIND_SETTINGS_RUMBLE, "Rumble", false, {}},
    {MenuKind::MENU_KIND_SETTINGS_SOUND, "Sound", false, {}},
    {MenuKind::MENU_KIND_DISPLAY, "Screen Display", false, {}},
    {MenuKind::MENU_KIND_SETTINGS_LANG, "Language", false, {}},
    {MenuKind::MENU_KIND_SETTINGS_ERASE, "Erase Data", false, {}},
    {MenuKind::MENU_KIND_DATA_SNAP, "Snapshots", false, {}},
    {MenuKind::MENU_KIND_DATA_ARCHIVES, "Archives", false, {}},
    {MenuKind::MENU_KIND_27, "Sound Test", false, {}},
    {MenuKind::MENU_KIND_DATA_SPECIAL, "Special", false, {}},
    {MenuKind::MENU_KIND_RECORDS_VS, "VS. Records", false, {}},
    {MenuKind::MENU_KIND_RECORDS_BONUS, "Bonus Records", false, {}},
    {MenuKind::MENU_KIND_RECORDS_MISC, "Misc. Records", false, {}},
};

/* Row names are drawn in capitals ("CHANNEL") and written here in normal
 * capitalisation. The Multi-Man Melee choices are the names on its cards, in
 * cursor order. */
constexpr ReaderRow kSoundRows[] = {
    {187, "Channel", RowValue::channel},
    {188, "Volume", RowValue::balance},
};
constexpr ReaderRow kDisplayRows[] = {
    {189, "Deflicker", RowValue::deflicker},
};
constexpr ReaderRow kMultiManChoices[] = {
    {171, "10-Man Melee", RowValue::none},
    {172, "100-Man Melee", RowValue::none},
    {173, "3-Minute Melee", RowValue::none},
    {174, "15-Minute Melee", RowValue::none},
    {175, "Endless Melee", RowValue::none},
    {176, "Cruel Melee", RowValue::none},
};

/* The rules screens' rows by their numbers in the hovered entry, in cursor
 * order. Additional Rules hides Score Display and Random Stage until they are
 * unlocked (mnRulePlus_IsOptionVisible); the cursor skips them then, so the
 * reader meets only the rows the game shows. */
constexpr ReaderRow kStockRow = {1, "Stock", RowValue::stock_count};
constexpr ReaderRow kCustomRulesRows[] = {
    {0, "Rules", RowValue::rules_mode, true},
    {1, "Time limit", RowValue::time_limit, false, -1, &kStockRow},
    {2, "Handicap", RowValue::handicap, true},
    {3, "Damage ratio", RowValue::damage_ratio},
    {4, "Stage selection", RowValue::stage_selection, true},
    {5, "Item Switch", RowValue::none, false, kind(MenuKind::MENU_KIND_RULES_ITEMS)},
    {6, "Additional Rules", RowValue::none, false, kind(MenuKind::MENU_KIND_RULES_EXTRA)},
};
constexpr ReaderRow kAdditionalRulesRows[] = {
    {0, "Stock match time limit", RowValue::stock_time_limit},
    {1, "Friendly fire", RowValue::friendly_fire, true},
    {2, "Pause", RowValue::pause, true},
    {3, "Score display", RowValue::score_display, true},
    {4, "Self-destructs", RowValue::self_destructs, true},
    {5, "Random Stage", RowValue::none, false, kind(MenuKind::MENU_KIND_RULES_STAGE)},
};

/* Item Switch's rows: the left column (0 to 15) and the right (16 to 30), each
 * top to bottom, then the frequency strip above them, a row of its own for
 * each column it is entered from. The names are drawn as two game texts, one
 * per column (SdMenu strings 2 and 3); mnItemSw_803ED438 maps each row to its
 * item. */
constexpr std::string_view kFrequencyHint = "Left and right to adjust.";
constexpr ReaderRow kItemSwitchRows[] = {
    {0, "Food", RowValue::switched},
    {1, "Maxim Tomato", RowValue::switched},
    {2, "Heart Container", RowValue::switched},
    {3, "Warp Star", RowValue::switched},
    {4, "Ray Gun", RowValue::switched},
    {5, "Super Scope", RowValue::switched},
    {6, "Fire Flower", RowValue::switched},
    {7, "Lip's Stick", RowValue::switched},
    {8, "Star Rod", RowValue::switched},
    {9, "Beam Sword", RowValue::switched},
    {10, "Home-Run Bat", RowValue::switched},
    {11, "Fan", RowValue::switched},
    {12, "Hammer", RowValue::switched},
    {13, "Green Shell", RowValue::switched},
    {14, "Red Shell", RowValue::switched},
    {15, "Flipper", RowValue::switched},
    {16, "Freezie", RowValue::switched},
    {17, "Mr. Saturn", RowValue::switched},
    {18, "Poké Ball", RowValue::switched},
    {19, "Bob-omb", RowValue::switched},
    {20, "Motion-Sensor Bomb", RowValue::switched},
    {21, "Super Mushroom", RowValue::switched},
    {22, "Poison Mushroom", RowValue::switched},
    {23, "Starman", RowValue::switched},
    {24, "Parasol", RowValue::switched},
    {25, "Screw Attack", RowValue::switched},
    {26, "Metal Box", RowValue::switched},
    {27, "Bunny Hood", RowValue::switched},
    {28, "Cloaking Device", RowValue::switched},
    {29, "Barrel Cannon", RowValue::switched},
    {30, "Party Ball", RowValue::switched},
    {.key = 31,
        .name = "Item frequency",
        .value = RowValue::item_frequency,
        .hint = kFrequencyHint},
    {.key = 32,
        .name = "Item frequency",
        .value = RowValue::item_frequency,
        .hint = kFrequencyHint},
};

constexpr ReaderRow stage_switch(int row, A11yStage stage) {
    return {.key = row, .value = RowValue::switched, .stage = stage};
}

/* Random Stage's rows: the left column (0 to 14) and the right (15 to 28),
 * each top to bottom. A copy of mnStageSw_803ED4C4 (static in mnstagesw.c),
 * which maps each row to its place in the stage switch, carried through to
 * the stage by gm_801641CC's table (lbl_803B7808 in gm_1601.c). The cursor
 * skips locked stages. */
constexpr ReaderRow kRandomStageRows[] = {
    stage_switch(0, A11Y_St_Kind_Castle),
    stage_switch(1, A11Y_St_Kind_Kongo),
    stage_switch(2, A11Y_St_Kind_GreatBay),
    stage_switch(3, A11Y_St_Kind_Zebes),
    stage_switch(4, A11Y_St_Kind_Story),
    stage_switch(5, A11Y_St_Kind_Izumi),
    stage_switch(6, A11Y_St_Kind_Corneria),
    stage_switch(7, A11Y_St_Kind_PStadium),
    stage_switch(8, A11Y_St_Kind_MuteCity),
    stage_switch(9, A11Y_St_Kind_Onett),
    stage_switch(10, A11Y_St_Kind_Inishie1),
    stage_switch(11, A11Y_St_Kind_Icemt),
    stage_switch(12, A11Y_St_Kind_Battle),
    stage_switch(13, A11Y_St_Kind_OldPupupu),
    stage_switch(14, A11Y_St_Kind_OldKongo),
    stage_switch(15, A11Y_St_Kind_RCruise),
    stage_switch(16, A11Y_St_Kind_Garden),
    stage_switch(17, A11Y_St_Kind_Shrine),
    stage_switch(18, A11Y_St_Kind_Kraid),
    stage_switch(19, A11Y_St_Kind_Yoster),
    stage_switch(20, A11Y_St_Kind_Greens),
    stage_switch(21, A11Y_St_Kind_Venom),
    stage_switch(22, A11Y_St_Kind_Pura),
    stage_switch(23, A11Y_St_Kind_BigBlue),
    stage_switch(24, A11Y_St_Kind_Fourside),
    stage_switch(25, A11Y_St_Kind_Inishie2),
    stage_switch(26, A11Y_St_Kind_Flatzone),
    stage_switch(27, A11Y_St_Kind_Last),
    stage_switch(28, A11Y_St_Kind_OldYoshi),
};

constexpr std::string_view kSwitchHint =
    "Up and down to move, left and right to change column, A to turn on or off.";

struct ScreenReader {
    MenuKind menu;
    Reader reader;
};

constexpr ScreenReader kReaders[] = {
    {MenuKind::MENU_KIND_SETTINGS_SOUND,
        {"Left and right to change.", RowKey::center_text, kSoundRows}},
    {MenuKind::MENU_KIND_DISPLAY, {"A to change.", RowKey::center_text, kDisplayRows}},
    {MenuKind::MENU_KIND_MULTI_VS,
        {"Left and right to choose.", RowKey::center_text, kMultiManChoices}},
    {MenuKind::MENU_KIND_RULES, {"Left and right to change.", RowKey::hovered, kCustomRulesRows}},
    {MenuKind::MENU_KIND_RULES_EXTRA,
        {"Left and right to change.", RowKey::hovered, kAdditionalRulesRows}},
    {MenuKind::MENU_KIND_RULES_ITEMS,
        {.hint = kSwitchHint, .key = RowKey::hovered, .rows = kItemSwitchRows, .described = false}},
    /* Turning off the last stage that is on plays the refused sound. */
    {MenuKind::MENU_KIND_RULES_STAGE, {.hint = kSwitchHint,
                                          .key = RowKey::hovered,
                                          .rows = kRandomStageRows,
                                          .described = false,
                                          .refused = "At least one stage must stay on."}},
};

/* Values as the rules screens draw them, by their stored numbers. */
constexpr std::string_view kRulesModeWords[] = {"Time", "Stock", "Coin", "Bonus"};
constexpr std::string_view kHandicapWords[] = {"Off", "Auto", "On"};
constexpr std::string_view kStageSelectionWords[] = {"On", "Random", "Ordered", "Turns", "Loser"};
constexpr std::string_view kSelfDestructsWords[] = {"minus 1", "0", "minus 2"};
constexpr std::string_view kSwitchWords[] = {"off", "on"};
constexpr std::string_view kItemFrequencyWords[] = {
    "None", "Very low", "Low", "Medium", "High", "Very high"};
static_assert(std::size(kRulesModeWords) == static_cast<std::size_t>(RulesMode::Mode_Bonus) + 1);
static_assert(std::size(kStageSelectionWords) ==
              static_cast<std::size_t>(StageSelectMode::StageSelectMode_Loser) + 1);

std::optional<std::string_view> word(std::span<const std::string_view> words, int number) {
    if (number < 0 || number >= static_cast<int>(words.size())) {
        return std::nullopt;
    }
    return words[static_cast<std::size_t>(number)];
}

const ScreenName* find_screen(int menu) {
    for (const ScreenName& screen : kScreens) {
        if (static_cast<int>(screen.menu) == menu) {
            return &screen;
        }
    }
    return nullptr;
}

/* Kept in capitals by plain_capitals. */
constexpr std::string_view kAbbreviations[] = {"LAN"};

bool is_abbreviation(std::string_view word) {
    for (std::string_view abbreviation : kAbbreviations) {
        if (word == abbreviation) {
            return true;
        }
    }
    return false;
}

char to_lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

}  // namespace

std::optional<std::string_view> menu_screen_name(int menu) {
    const ScreenName* screen = find_screen(menu);
    if (screen == nullptr) {
        return std::nullopt;
    }
    return screen->name;
}

bool is_tree_screen(int menu) {
    const ScreenName* screen = find_screen(menu);
    return screen != nullptr && screen->tree;
}

std::optional<std::string_view> menu_entry_name(int menu, int entry) {
    const ScreenName* screen = find_screen(menu);
    if (screen == nullptr) {
        return std::nullopt;
    }
    for (const EntryName& name : screen->entries) {
        if (name.entry == entry) {
            return name.name;
        }
    }
    return std::nullopt;
}

std::string_view ReaderRow::spoken_name() const {
    if (stage != A11Y_NO_STAGE) {
        return stage_name(stage).value_or(kUnknownStage);
    }
    return name;
}

const ReaderRow* Reader::row(int key) const {
    for (const ReaderRow& row : rows) {
        if (row.key == key) {
            return &row;
        }
    }
    return nullptr;
}

bool Reader::opens(int menu) const {
    for (const ReaderRow& row : rows) {
        if (row.opens == menu) {
            return true;
        }
    }
    return false;
}

const Reader* menu_reader(int menu) {
    for (const ScreenReader& screen : kReaders) {
        if (static_cast<int>(screen.menu) == menu) {
            return &screen.reader;
        }
    }
    return nullptr;
}

std::string_view channel_word(bool mono) {
    return mono ? "Mono" : "Stereo";
}

/* The bar is split between a SOUNDS part on the left and a MUSIC part on the
 * right. Right moves the divider toward the MUSIC label but grows the SOUNDS
 * part: sound effects louder, music quieter, silent at +100. So a positive
 * balance leans toward sounds, whichever way the divider moved. */
std::string balance_words(int balance) {
    if (balance == 0) {
        return "centre";
    }
    int distance = balance < 0 ? -balance : balance;
    return std::to_string(distance) + (balance < 0 ? " toward music" : " toward sounds");
}

std::string_view on_off_word(bool on) {
    return on ? "On" : "Off";
}

bool is_stock_match(int mode) {
    return mode == static_cast<int>(RulesMode::Mode_Stock);
}

std::optional<std::string_view> rules_mode_word(int mode) {
    return word(kRulesModeWords, mode);
}

std::string minutes_words(int minutes) {
    if (minutes == 0) {
        return "none";
    }
    return std::to_string(minutes) + (minutes == 1 ? " minute" : " minutes");
}

std::optional<std::string_view> handicap_word(int handicap) {
    return word(kHandicapWords, handicap);
}

std::string damage_ratio_words(int tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::optional<std::string_view> stage_selection_word(int stage_selection) {
    return word(kStageSelectionWords, stage_selection);
}

std::optional<std::string_view> self_destructs_words(int stored) {
    return word(kSelfDestructsWords, stored);
}

std::optional<std::string_view> switch_word(int on) {
    return word(kSwitchWords, on);
}

std::optional<std::string_view> item_frequency_word(int frequency) {
    return word(kItemFrequencyWords, frequency);
}

std::string plain_capitals(std::string_view label) {
    for (char c : label) {
        if (c >= 'a' && c <= 'z') {
            return std::string(label);
        }
    }
    std::string out(label);
    bool first_word = true;
    for (std::size_t start = 0; start < label.size();) {
        std::size_t end = label.find(' ', start);
        if (end == std::string_view::npos) {
            end = label.size();
        }
        if (end > start) {
            if (!is_abbreviation(label.substr(start, end - start))) {
                for (std::size_t i = first_word ? start + 1 : start; i < end; i++) {
                    out[i] = to_lower(out[i]);
                }
            }
            first_word = false;
        }
        start = end + 1;
    }
    return out;
}

}  // namespace a11y
