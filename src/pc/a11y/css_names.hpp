/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's string table: the characters' names as drawn under their
 * portraits on the English screen, in normal capitalisation ("C.FALCON" is
 * "C. Falcon"), so the blind player hears what a sighted player reads
 * (.scratch/character-select/spec.md, "Where the words come from"). The game
 * draws them as pictures, so there is no game text to read. Every other word
 * character select speech says is here too. */
#pragma once
#include "character_kinds.h"
#include "css_kinds.h"
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
/* A held slider's value is said on every step, so it gets a shorter word than
 * the knob's name: "Level 4". */
inline constexpr std::string_view kCpuLevelHeld = "Level";
inline constexpr std::string_view kHandicapHeld = "Handicap";

/* The screens character select opens inside itself, as the main menu tree
 * names them, and the name tag window. */
inline constexpr std::string_view kRulesScreen = "Custom Rules";
inline constexpr std::string_view kNameEntryScreen = "Name Entry";
inline constexpr std::string_view kNameTagsWindow = "Name tags";

/* The rest of what character select says, around the names above. */
inline constexpr std::string_view kScreenName = "Character select";
inline constexpr std::string_view kPlayer = "Player";
inline constexpr std::string_view kTeam = "team";
inline constexpr std::string_view kOn = "on";
inline constexpr std::string_view kOff = "off";
inline constexpr std::string_view kNoCharacter = "no character";
inline constexpr std::string_view kUnknownCharacter = "Unknown character";
inline constexpr std::string_view kCostume = "Costume";
inline constexpr std::string_view kHoldingYourCoin = "Holding your coin";
/* "Holding player 2's coin", around the player's number. */
inline constexpr std::string_view kHoldingCoinBefore = "Holding player ";
inline constexpr std::string_view kHoldingCoinAfter = "'s coin";
inline constexpr std::string_view kBackTo = "Back to";
inline constexpr std::string_view kHoldingSlider = "Holding the slider";
inline constexpr std::string_view kReleased = "Released";
inline constexpr std::string_view kReadyToFight = "Ready to fight. Press Start.";
inline constexpr std::string_view kNoSpeechYet = "No speech yet.";
/* Steering: a step with nowhere to go, and a glide that did not get there
 * ("Could not reach Fox"). */
inline constexpr std::string_view kNothingThatWay = "Nothing that way";
inline constexpr std::string_view kCouldNotReach = "Could not reach";

}  // namespace a11y
