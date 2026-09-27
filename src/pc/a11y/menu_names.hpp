/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The main menu tree's string table: the names of its menu screens and of
 * the entries of its tree screens, as drawn on the English screen
 * (.scratch/main-menu-tree/spec.md, "The string table"). The game draws them
 * as pictures, so there is no game text to read. */
#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace a11y {

/* A menu screen's name: a tree screen's title, a leaf screen's name as it
 * calls itself. Nothing for a MenuKind missing from the table. */
std::optional<std::string_view> menu_screen_name(int menu);

/* Whether the screen is a tree screen: a list of entries whose hovered entry
 * is spoken as the cursor moves. Leaf screens and unknown screens are not. */
bool is_tree_screen(int menu);

/* An entry's name on a tree screen. Nothing for an entry missing from the
 * table, including every entry of Online, which the base port names. */
std::optional<std::string_view> menu_entry_name(int menu, int entry);

/* A label the base port draws in capitals ("DIRECT CONNECT"), in normal
 * capitalisation ("Direct connect"), keeping abbreviations such as LAN. A
 * label with any lower-case letter is returned as it is. */
std::string plain_capitals(std::string_view label);

}  // namespace a11y
