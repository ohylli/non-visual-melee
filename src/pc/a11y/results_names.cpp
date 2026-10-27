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

std::string port_list(const std::vector<int>& ports) {
    std::string out;
    for (std::size_t i = 0; i < ports.size(); i++) {
        if (i > 0) {
            out += i + 1 == ports.size() ? " and " : ", ";
        }
        out += port_name(ports[i]);
    }
    return out;
}

std::optional<std::string_view> results_page_name(int page) {
    constexpr std::string_view kPages[] = {"KOs and falls", "Stats", "Bonuses"};
    if (page < 0 || page >= static_cast<int>(std::size(kPages))) {
        return std::nullopt;
    }
    return kPages[page];
}

std::string stat_label(std::string_view label) {
    auto upper = [](char c) { return c >= 'A' && c <= 'Z'; };
    auto lower = [](char c) { return c >= 'a' && c <= 'z'; };
    std::string out;
    for (std::size_t i = 0; i < label.size(); i++) {
        char c = label[i];
        char next = i + 1 < label.size() ? label[i + 1] : '\0';
        if (i > 0 && upper(c)) {
            char previous = label[i - 1];
            if (lower(previous)) {
                out += ' ';
            }
            /* A word's capital, not an abbreviation's ("KOs"). */
            if ((lower(previous) || previous == ' ') && lower(next)) {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        out += c;
    }
    return out;
}

}  // namespace a11y
