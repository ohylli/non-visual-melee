/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The main menu tree's string table: the names of its menu screens, of the
 * entries of its tree screens, and of the rows, choices and values of the
 * leaf screens with a reader, as drawn on the English screen
 * (.scratch/main-menu-tree/spec.md, "The string table"). The game draws them
 * as pictures, so there is no game text to read. */
#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace a11y {

/* The leaf screens with a reader, by the decomp's MenuKind (menu_kinds.h). */
constexpr int kMenuSound = 20;
constexpr int kMenuDisplay = 21;
constexpr int kMenuMultiMan = 33;

/* The centre texts those screens set (Menu_InitCenterText), by the SdMenu
 * numbers the game's code passes: the NTSC-U ones, remapped for PAL discs
 * only when the text is looked up. */
constexpr int kTextSoundChannel = 187;
constexpr int kTextSoundVolume = 188;
constexpr int kTextDisplay = 189;
/* The six Multi-Man Melee choices in cursor order: 171 to 176. */
constexpr int kTextMultiManFirst = 171;

/* A menu screen's name: a tree screen's title, a leaf screen's name as it
 * calls itself. Nothing for a MenuKind missing from the table. */
std::optional<std::string_view> menu_screen_name(int menu);

/* Whether the screen is a tree screen: a list of entries whose hovered entry
 * is spoken as the cursor moves. Leaf screens and unknown screens are not. */
bool is_tree_screen(int menu);

/* An entry's name on a tree screen. Nothing for an entry missing from the
 * table, including every entry of Online, which the base port names. */
std::optional<std::string_view> menu_entry_name(int menu, int entry);

/* The name of what a leaf screen's centre text describes: the Sound row
 * ("Channel", "Volume"), Screen display's one row ("Deflicker") or the
 * Multi-Man Melee choice ("10-Man Melee"). Nothing for a centre text the table
 * does not give that screen. */
std::optional<std::string_view> center_text_name(int menu, int center_text);

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
