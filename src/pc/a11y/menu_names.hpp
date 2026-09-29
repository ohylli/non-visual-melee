/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The main menu tree's string table: the names of its menu screens, of the
 * entries of its tree screens, and of the rows, choices and values of the
 * leaf screens with a reader, as drawn on the English screen
 * (.scratch/main-menu-tree/spec.md, "The string table"). The game draws them
 * as pictures, so there is no game text to read. */
#pragma once
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace a11y {

/* The setting a reader's row shows beside its name. */
enum class RowValue { none, channel, balance, deflicker };

/* A row or choice of a leaf screen with a reader, by the centre text the
 * screen sets for it (Menu_InitCenterText): the SdMenu number the game's code
 * passes, the NTSC-U one, remapped for PAL discs only when the text is looked
 * up. */
struct ReaderRow {
    int center_text;
    std::string_view name;
    RowValue value;
};

/* A leaf screen with a reader: the key hint spoken once as it opens, and the
 * rows or choices its centre text names. */
struct Reader {
    std::string_view hint;
    std::span<const ReaderRow> rows;

    /* The row the centre text names; nothing for one this reader lacks. */
    const ReaderRow* row(int center_text) const;
};

/* The reader of a leaf screen: Sound, Screen display or Multi-Man Melee.
 * Nothing for any other screen. */
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

/* A label the base port draws in capitals ("DIRECT CONNECT"), in normal
 * capitalisation ("Direct connect"), keeping abbreviations such as LAN. A
 * label with any lower-case letter is returned as it is. */
std::string plain_capitals(std::string_view label);

}  // namespace a11y
