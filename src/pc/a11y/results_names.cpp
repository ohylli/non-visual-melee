/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "results_names.hpp"
#include <iterator>

namespace a11y {

std::string place_word(int place) {
    int number = place + 1;
    const char* suffix = "th";
    if (number % 100 < 11 || number % 100 > 13) {
        switch (number % 10) {
        case 1:
            suffix = "st";
            break;
        case 2:
            suffix = "nd";
            break;
        case 3:
            suffix = "rd";
            break;
        default:
            break;
        }
    }
    return std::to_string(number) + suffix;
}

std::optional<std::string_view> results_team_name(int team) {
    /* The player's team, 0 to 2, as character select's team button shows it
     * (css_names.cpp, team_word); the summary's colours agree
     * (fn_8017507C). */
    constexpr std::string_view kTeams[] = {"Red team", "Blue team", "Green team"};
    if (team < 0 || team >= static_cast<int>(std::size(kTeams))) {
        return std::nullopt;
    }
    return kTeams[team];
}

std::string count_of(int count, std::string_view one, std::string_view many) {
    std::string out = std::to_string(count);
    out += ' ';
    out += count == 1 ? one : many;
    return out;
}

std::string port_name(int port) {
    return "P" + std::to_string(port + 1);
}

}  // namespace a11y
