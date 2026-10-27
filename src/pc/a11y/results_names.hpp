/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The words results speech says around the game's own text
 * (.scratch/results/spec.md): the summary table draws the winner, the places
 * and the characters as pictures, so their words are the fork's. The match
 * type is the game's text (SdRst), read by results_speech.cpp. */
#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

/* A place as its picture shows it, from 0: "1st", "2nd", "3rd", "4th". */
std::string place_word(int place);

/* A team by its colour, as the winner's banner names it: "Red team", "Blue
 * team", "Green team"; nothing for a number the game does not use. */
std::optional<std::string_view> results_team_name(int team);

/* A count with its noun, singular for one: "1 KO", "2 KOs", "0 falls". */
std::string count_of(int count, std::string_view one, std::string_view many);

/* A port as the summary names it: "P1" for port 0. */
std::string port_name(int port);

/* Ports as a list said aloud: "P2", "P2 and P3", "P2, P3 and P4". */
std::string port_list(const std::vector<int>& ports);

/* A stats panel page by its number, as the fork names it (the screen shows
 * no title): "KOs and falls", "Stats", "Bonuses"; nothing for another. */
std::optional<std::string_view> results_page_name(int page);

/* A stat's label as the screen draws it, made speakable: a space before each
 * capital that follows a lower-case letter, and each word after the first
 * lower-cased unless it is all capitals. "DamageGiven" is "Damage given",
 * "Attempted Grabs" "Attempted grabs", "KOs" stays. */
std::string stat_label(std::string_view label);

inline constexpr std::string_view kWinner = "Winner: ";
inline constexpr std::string_view kNoContest = "No contest";
inline constexpr std::string_view kYou = "you";
inline constexpr std::string_view kNotKnockedOut = "not KO'd";
inline constexpr std::string_view kKnockedOutAt = "KO'd at ";
inline constexpr std::string_view kTotal = "total ";
/* Said once, on the first results of each game run, before the last
 * sentence. */
inline constexpr std::string_view kOpeningKeys = "Any button shows the stats.";
inline constexpr std::string_view kStartWhenReady = "Start when ready.";
/* Said after the stats panels' first announcement, on the first results of
 * each game run. */
inline constexpr std::string_view kStatsKeys =
    "A and B change page. D-pad up and down read the rows, left and right switch player. Z the "
    "player's summary. Start when ready.";
/* Ready: yours with whom you wait for ("Ready, waiting for P2."), your
 * un-ready, and another human's ("P2 ready.", "P2 not ready."); online the
 * other human is your opponent. */
inline constexpr std::string_view kReadyWaitingFor = "Ready, waiting for ";
inline constexpr std::string_view kReadyWaitingForOpponent = "Ready, waiting for your opponent.";
inline constexpr std::string_view kNotReady = "Not ready.";
inline constexpr std::string_view kOtherReady = " ready.";
inline constexpr std::string_view kOtherNotReady = " not ready.";
inline constexpr std::string_view kOpponentReady = "Your opponent is ready.";
inline constexpr std::string_view kOpponentNotReady = "Your opponent is not ready.";
/* A stat the screen draws as "---". */
inline constexpr std::string_view kNoValue = "none";
/* A Bonuses page with no row. */
inline constexpr std::string_view kNoBonuses = "No bonuses";

}  // namespace a11y
