/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_names.hpp"
#include "menu_kinds.h"
#include <span>

namespace a11y {
namespace {

enum class MenuKind : int {
#define A11Y_MENU_KIND_ENUMERATOR(name, number) name = number,
    A11Y_MENU_KINDS(A11Y_MENU_KIND_ENUMERATOR)
#undef A11Y_MENU_KIND_ENUMERATOR
};

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
