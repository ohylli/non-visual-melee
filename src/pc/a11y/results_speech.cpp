/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "results_speech.hpp"
#include "css_names.hpp"
#include "pc/pc.h"
#include "results_names.hpp"
#include "speech.hpp"
#include <algorithm>
#include <cstdio>
#include <string_view>
#include <tuple>

namespace a11y {
namespace {

bool has_port(const A11yResults& results, int port) {
    return port >= 0 && port < A11Y_RESULTS_PORTS &&
           results.players[port].slot != A11Y_RESULTS_EMPTY;
}

/* Parts of one announcement as sentences: "Stock Battle. Winner: DK." A
 * part that ends a sentence itself keeps its own mark. */
void add_sentence(std::string& out, std::string_view part) {
    if (part.empty()) {
        return;
    }
    if (!out.empty()) {
        char last = out.back();
        if (last != '.' && last != '!' && last != '?') {
            out += '.';
        }
        out += ' ';
    }
    out += part;
}

void add_part(std::string& out, std::string_view part) {
    if (part.empty()) {
        return;
    }
    if (!out.empty()) {
        out += ", ";
    }
    out += part;
}

std::string character_word(const A11yResults& results, int port) {
    if (std::optional<std::string_view> name = character_name(results.players[port].character)) {
        return std::string(*name);
    }
    /* Unknown: the port alone names the player. */
    return "";
}

/* The summary's last row, by the match type; empty where the screen shows
 * none or only a dash (Bonus mode's points on a cancelled match). */
std::string last_row(const A11yResults& results, const A11yResultsPlayer& player) {
    switch (results.match_kind) {
    case A11Y_RESULTS_STOCK:
        if (!player.knocked_out) {
            return std::string(kNotKnockedOut);
        } else {
            char time[16];
            std::snprintf(
                time, sizeof(time), "%d:%02d", player.ko_seconds / 60, player.ko_seconds % 60);
            return std::string(kKnockedOutAt) + time;
        }
    case A11Y_RESULTS_TIME:
        return std::string(kTotal) + (player.score > 0 ? "+" : "") + std::to_string(player.score);
    case A11Y_RESULTS_COIN:
        return count_of(player.coins, "coin", "coins");
    case A11Y_RESULTS_BONUS:
        return results.cancelled ? "" : count_of(player.score, "point", "points");
    }
    return "";
}

}  // namespace

int results_you(const A11yResults& results) {
    int port = results.local_port;
    if (!has_port(results, port) || results.players[port].slot != A11Y_RESULTS_HUMAN) {
        return -1;
    }
    return port;
}

std::vector<int> results_order(const A11yResults& results) {
    std::vector<int> ports;
    for (int port = 0; port < A11Y_RESULTS_PORTS; port++) {
        if (has_port(results, port)) {
            ports.push_back(port);
        }
    }
    if (results.cancelled) {
        return ports;
    }
    auto key = [&results](int port) {
        const A11yResultsPlayer& player = results.players[port];
        return std::make_tuple(player.place, port != results.winner, player.rank, port);
    };
    std::sort(ports.begin(), ports.end(), [&key](int a, int b) { return key(a) < key(b); });
    return ports;
}

std::string results_player_name(const A11yResults& results, int port) {
    std::string out = character_word(results, port);
    add_part(out, port_name(port));
    if (port == results_you(results)) {
        add_part(out, kYou);
    }
    return out;
}

std::string results_summary(const A11yResults& results, int port) {
    const A11yResultsPlayer& player = results.players[port];
    std::string out;
    if (results.match_kind != A11Y_RESULTS_BONUS) {
        add_part(out, count_of(player.kos, "KO", "KOs"));
        add_part(out, count_of(player.falls, "fall", "falls"));
        add_part(out, count_of(player.sds, "SD", "SDs"));
    }
    add_part(out, last_row(results, player));
    return out;
}

void ResultsSpeech::visit_started() {
    m_opened = false;
}

void ResultsSpeech::tick(const A11yResults& results) {
    if (!results.valid) {
        return;
    }
    if (!m_opened && results.state >= A11Y_RESULTS_SUMMARY) {
        opening(results);
    }
}

void ResultsSpeech::opening(const A11yResults& results) {
    m_opened = true;
    std::string text;
    add_sentence(text, match_type(results));
    if (results.cancelled) {
        add_sentence(text, kNoContest);
    } else if (results.teams) {
        std::optional<std::string_view> team = results_team_name(results.winning_team);
        if (team) {
            add_sentence(text, std::string(kWinner) + std::string(*team));
        } else {
            log_once("[a11y] results: winning team " + std::to_string(results.winning_team) +
                     " not in the results names table");
        }
    } else if (has_port(results, results.winner)) {
        std::string name = character_word(results, results.winner);
        add_sentence(
            text, std::string(kWinner) + (name.empty() ? port_name(results.winner) : name));
    }

    for (int port : results_order(results)) {
        const A11yResultsPlayer& player = results.players[port];
        if (player.character == A11Y_NO_CHARACTER) {
            log_once("[a11y] results: port " + std::to_string(port) +
                     ": character not in the character names table");
        }
        std::string line = results.cancelled ? "" : place_word(player.place) + " ";
        line += results_player_name(results, port);
        if (results.teams) {
            if (std::optional<std::string_view> team = results_team_name(player.team)) {
                add_part(line, *team);
            }
        }
        add_part(line, results_summary(results, port));
        add_sentence(text, line);
    }

    if (m_keys_due) {
        add_sentence(text, kOpeningKeys);
        m_keys_due = false;
    }
    add_sentence(text, kStartWhenReady);
    m_speech.announce(text, Mode::interrupt);
}

std::string ResultsSpeech::match_type(const A11yResults& results) {
    if (results.match_type_text == nullptr) {
        log_once(
            "[a11y] results: match type " + std::to_string(results.match_kind) + ": no game text");
        return "";
    }
    DecodedText decoded = decode_game_text(results.match_type_text, m_text);
    if (decoded.unknown_glyphs > 0 || decoded.malformed) {
        log_once("[a11y] results: match type \"" + one_line(decoded.text) + "\"" +
                 (decoded.unknown_glyphs > 0 ? ", unknown glyphs left out" : "") +
                 (decoded.malformed ? ", malformed" : ""));
    }
    return spoken_text(decoded.text);
}

void ResultsSpeech::log_once(const std::string& line) {
    if (m_logged.insert(line).second && log_enabled()) {
        pc_log_line("%s", line.c_str());
    }
}

}  // namespace a11y
