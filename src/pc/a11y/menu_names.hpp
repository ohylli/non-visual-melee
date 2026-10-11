/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The main menu tree's string table: the names of its menu screens, of the
 * entries of its tree screens, and of the rows, choices and values of the
 * leaf screens with a reader, as drawn on the English screen
 * (.scratch/main-menu-tree/spec.md, "The string table";
 * .scratch/rules-screens/spec.md for the rules screens). The game draws them
 * as pictures, so there is no game text to read. */
#pragma once
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace a11y {

/* The setting a reader's row shows beside its name. */
enum class RowValue {
    none,
    /* Sound and Screen display. */
    channel,
    balance,
    deflicker,
    /* Custom Rules. */
    rules_mode,
    time_limit,
    stock_count,
    handicap,
    damage_ratio,
    stage_selection,
    /* Additional Rules. */
    stock_time_limit,
    friendly_fire,
    pause,
    score_display,
    self_destructs,
};

/* What tells a reader's rows apart. Sound, Screen display and Multi-Man
 * Melee set a centre text per row (Menu_InitCenterText): the SdMenu number
 * the game's code passes, the NTSC-U one, remapped for PAL discs only when
 * the text is looked up. The rules screens keep the row in the hovered entry
 * (mn_804A04F0.hovered_selection). */
enum class RowKey { center_text, hovered };

/* A row or choice of a leaf screen with a reader. */
struct ReaderRow {
    /* The centre text or hovered entry that names it, by the reader's
     * RowKey. */
    int key;
    std::string_view name;
    RowValue value;
    /* The row's description changes with its value, so a value change says
     * it after the value. */
    bool described_per_value = false;
    /* The menu screen (MenuKind) A on the row opens, or -1. */
    int opens = -1;
    /* The row as a stock match shows it, where that renames it: Custom
     * Rules' Time Limit becomes Stock. */
    const ReaderRow* in_stock_match = nullptr;
};

/* A leaf screen with a reader: the key hint spoken once as it opens, and its
 * rows or choices. */
struct Reader {
    std::string_view hint;
    RowKey key;
    std::span<const ReaderRow> rows;

    /* The row the key names; nothing for one this reader lacks. */
    const ReaderRow* row(int key) const;
    /* Whether one of its rows opens the menu screen. */
    bool opens(int menu) const;
};

/* The reader of a leaf screen: Sound, Screen display, Multi-Man Melee,
 * Custom Rules or Additional Rules. Nothing for any other screen. */
const Reader* menu_reader(int menu);

/* A menu screen's name: a tree screen's title, a leaf screen's name as it
 * calls itself. Nothing for a MenuKind missing from the table. */
std::optional<std::string_view> menu_screen_name(int menu);

/* Whether the screen is a tree screen: a list of entries whose hovered entry
 * is spoken as the cursor moves. Leaf screens and unknown screens are not. */
bool is_tree_screen(int menu);

/* An entry's name on a tree screen. Nothing for an entry missing from the
 * table, including every entry of Online, which the base port names. */
std::optional<std::string_view> menu_entry_name(int menu, int entry);

/* Values as the screens draw them. The balance runs from -100 (music only) to
 * +100 (sounds only) and is spoken as its distance from the centre and the
 * side it leans to: "15 toward sounds", "centre". */
std::string_view channel_word(bool mono);
std::string balance_words(int balance);
std::string_view on_off_word(bool on);

/* The rules screens' values (A11yRules). Nothing for a number outside the
 * screen's range. Minutes are "none" for no limit, "1 minute", "5 minutes";
 * the damage ratio, kept in tenths, has one decimal always: "1.0". The
 * self-destruct value is as stored: 0 is "minus 1", 1 "0", 2 "minus 2". */
bool is_stock_match(int mode);
std::optional<std::string_view> rules_mode_word(int mode);
std::string minutes_words(int minutes);
std::optional<std::string_view> handicap_word(int handicap);
std::string damage_ratio_words(int tenths);
std::optional<std::string_view> stage_selection_word(int stage_selection);
std::optional<std::string_view> self_destructs_words(int stored);

/* A label the base port draws in capitals ("DIRECT CONNECT"), in normal
 * capitalisation ("Direct connect"), keeping abbreviations such as LAN. A
 * label with any lower-case letter is returned as it is. */
std::string plain_capitals(std::string_view label);

}  // namespace a11y
