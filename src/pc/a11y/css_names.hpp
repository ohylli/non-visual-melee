/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's string table: the characters' names as drawn under their
 * portraits on the English screen, in normal capitalisation ("C.FALCON" is
 * "C. Falcon"), so the blind player hears what a sighted player reads
 * (.scratch/character-select/spec.md, "Where the words come from"). The game
 * draws them as pictures, so there is no game text to read. */
#pragma once
#include "character_kinds.h"
#include "game_access.h"
#include <optional>
#include <string_view>

namespace a11y {

/* A character's name; nothing for none or a kind missing from the table. */
std::optional<std::string_view> character_name(A11yCharacter character);

/* A player slot's kind as a word, for the tab's "HMN", "CPU" and "N/A":
 * "human", "CPU", "closed". */
std::string_view slot_kind_word(A11yCssSlotKind kind);

/* A team by the colour its button shows: "red", "blue", "green"; nothing for
 * a number the game does not use. */
std::optional<std::string_view> team_word(int team);

/* The top bar's buttons, the sliders and the name box, as spoken. */
inline constexpr std::string_view kTeamsButton = "Teams";
inline constexpr std::string_view kRulesButton = "Rules";
inline constexpr std::string_view kBackButton = "Back";
inline constexpr std::string_view kCpuLevelSlider = "CPU level";
inline constexpr std::string_view kHandicapSlider = "handicap";
inline constexpr std::string_view kNameBox = "name tag";

/* The screens character select opens inside itself, as the main menu tree
 * names them. */
inline constexpr std::string_view kRulesScreen = "Custom Rules";
inline constexpr std::string_view kNameEntryScreen = "Name Entry";

}  // namespace a11y
