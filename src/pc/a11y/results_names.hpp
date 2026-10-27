/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The words results speech says around the game's own text
 * (.scratch/results/spec.md): the summary table draws the winner, the places
 * and the characters as pictures, so their words are the fork's. The match
 * type is the game's text (SdRst), read by results_speech.cpp. */
#pragma once
#include <optional>
#include <string>
#include <string_view>

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

}  // namespace a11y
