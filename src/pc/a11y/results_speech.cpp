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

/* Ends the text with a full stop unless it ends a sentence already. */
void end_sentence(std::string& out) {
    if (!out.empty() && out.back() != '.' && out.back() != '!' && out.back() != '?') {
        out += '.';
    }
}

/* The port whose panel the reader follows: controller 1's (yours, or a CPU
 * it pages), or else the first player's. */
int panel_port(const A11yResults& results) {
    if (has_port(results, results.local_port)) {
        return results.local_port;
    }
    for (int port = 0; port < A11Y_RESULTS_PORTS; port++) {
        if (has_port(results, port)) {
            return port;
        }
    }
    return -1;
}

/* The players the reading cursor switches between: you first, then the
 * others by port, CPUs included; by port when nobody is you. */
std::vector<int> cursor_players(const A11yResults& results) {
    std::vector<int> ports;
    int you = results_you(results);
    if (you >= 0) {
        ports.push_back(you);
    }
    for (int port = 0; port < A11Y_RESULTS_PORTS; port++) {
        if (port != you && has_port(results, port)) {
            ports.push_back(port);
        }
    }
    return ports;
}

}  // namespace

std::string ResultsRow::text() const {
    std::string out = label;
    if (has_value) {
        add_part(out, value);
    }
    return out;
}

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

void ResultsSpeech::visit_started() {
    m_opened = false;
    m_stats_open = false;
}

void ResultsSpeech::tick(const A11yResults& results) {
    if (!results.valid) {
        return;
    }
    bool opened_now = false;
    if (!m_opened && results.state >= A11Y_RESULTS_SUMMARY) {
        opening(results);
        opened_now = true;
    }
    if (results.state < A11Y_RESULTS_STATS || panel_port(results) < 0) {
        return;
    }
    if (!m_stats_open) {
        /* The press that opened the panels is only that: whatever it was,
         * it reads no row. */
        stats_opened(results, opened_now ? Mode::queue : Mode::interrupt);
        return;
    }
    int page = results.players[panel_port(results)].page;
    if (page != m_page) {
        /* Your page changed; a press at either end changes nothing and is
         * silent, as the game is. */
        m_page = page;
        m_row = 0;
        m_speech.announce(page_announcement(results), Mode::interrupt);
    }
    if (results.pressed_up) {
        step(results, -1);
    }
    if (results.pressed_down) {
        step(results, 1);
    }
    if (results.pressed_left) {
        switch_player(results, -1);
    }
    if (results.pressed_right) {
        switch_player(results, 1);
    }
    if (results.pressed_z) {
        summary_line(results);
    }
    ready_changes(results);
}

void ResultsSpeech::stats_opened(const A11yResults& results, Mode mode) {
    m_stats_open = true;
    m_page = results.players[panel_port(results)].page;
    m_port = cursor_players(results).front();
    m_row = 0;
    for (int port = 0; port < A11Y_RESULTS_PORTS; port++) {
        m_ready[port] = results.players[port].ready;
    }
    std::string text = page_announcement(results);
    if (m_stats_keys_due) {
        add_sentence(text, kStatsKeys);
        m_stats_keys_due = false;
    }
    m_speech.announce(text, mode);
}

std::string ResultsSpeech::page_announcement(const A11yResults& results) {
    std::string text;
    if (std::optional<std::string_view> name = results_page_name(m_page)) {
        text = std::string(*name);
    } else {
        log_once("[a11y] results: page " + std::to_string(m_page) + " has no name");
    }
    std::vector<ResultsRow> list = rows(results, m_port, m_page);
    add_sentence(text, list.empty() ? std::string(kNoBonuses) : list.front().text());
    end_sentence(text);
    return text;
}

void ResultsSpeech::step(const A11yResults& results, int by) {
    std::vector<ResultsRow> list = rows(results, m_port, m_page);
    if (list.empty()) {
        m_speech.announce(kNoBonuses, Mode::interrupt);
        return;
    }
    /* At the first or last row, the row again. */
    m_row = std::clamp(m_row + by, 0, static_cast<int>(list.size()) - 1);
    m_speech.announce(list[m_row].text(), Mode::interrupt);
}

void ResultsSpeech::switch_player(const A11yResults& results, int by) {
    std::vector<int> players = cursor_players(results);
    auto at = std::find(players.begin(), players.end(), m_port);
    int index = at == players.end() ? 0 : static_cast<int>(at - players.begin());
    int next = std::clamp(index + by, 0, static_cast<int>(players.size()) - 1);
    std::string text = results_player_name(results, players[next]);
    if (next == index && at != players.end()) {
        /* At either end, the edge player again. */
        m_speech.announce(text, Mode::interrupt);
        return;
    }
    std::vector<ResultsRow> from = rows(results, m_port, m_page);
    std::optional<ResultsRow> left;
    if (m_row < static_cast<int>(from.size())) {
        left = from[m_row];
    }
    m_port = players[next];
    std::vector<ResultsRow> to = rows(results, m_port, m_page);
    if (to.empty()) {
        add_part(text, kNoBonuses);
    } else {
        m_row = std::min(m_row, static_cast<int>(to.size()) - 1);
        const ResultsRow& row = to[m_row];
        /* Only what differs: the value alone where the label is the same,
         * the whole row otherwise; a heading is its label. */
        bool same_label = left && left->has_value && row.has_value && left->label == row.label;
        add_part(text, same_label ? row.value : row.text());
    }
    m_speech.announce(text, Mode::interrupt);
}

void ResultsSpeech::summary_line(const A11yResults& results) {
    const A11yResultsPlayer& player = results.players[m_port];
    std::string text = results_player_name(results, m_port);
    if (!results.cancelled) {
        add_part(text, place_word(player.place));
    }
    if (results.teams) {
        if (std::optional<std::string_view> team = results_team_name(player.team)) {
            add_part(text, *team);
        }
    }
    add_part(text, results_summary(results, m_port));
    end_sentence(text);
    m_speech.announce(text, Mode::interrupt);
}

void ResultsSpeech::ready_changes(const A11yResults& results) {
    int you = results_you(results);
    /* Every panel is ready and the scene leaves: the next screen speaks. */
    bool leaving = results.state == A11Y_RESULTS_LEAVING;
    std::string yours;
    std::vector<std::string> others;
    for (int port = 0; port < A11Y_RESULTS_PORTS; port++) {
        const A11yResultsPlayer& player = results.players[port];
        bool was_ready = m_ready[port];
        m_ready[port] = player.ready;
        /* CPUs are said nothing of, and nor is a human the game readied
         * because no controller is plugged in. */
        if (player.slot != A11Y_RESULTS_HUMAN || player.unplugged || player.ready == was_ready) {
            continue;
        }
        if (port == you) {
            if (!player.ready) {
                yours = kNotReady;
                continue;
            }
            std::vector<int> waiting;
            for (int other = 0; other < A11Y_RESULTS_PORTS; other++) {
                const A11yResultsPlayer& them = results.players[other];
                if (other != you && them.slot == A11Y_RESULTS_HUMAN && !them.ready) {
                    waiting.push_back(other);
                }
            }
            if (leaving || waiting.empty()) {
                continue;
            }
            yours = results.online ? std::string(kReadyWaitingForOpponent) :
                                     std::string(kReadyWaitingFor) + port_list(waiting) + ".";
        } else if (!player.ready) {
            others.push_back(results.online ? std::string(kOpponentNotReady) :
                                              port_name(port) + std::string(kOtherNotReady));
        } else if (!leaving) {
            others.push_back(results.online ? std::string(kOpponentReady) :
                                              port_name(port) + std::string(kOtherReady));
        }
    }
    if (!yours.empty()) {
        m_speech.announce(yours, Mode::interrupt);
    }
    for (const std::string& text : others) {
        m_speech.announce(text, Mode::queue);
    }
}

std::string ResultsSpeech::opponent_name(
    const A11yResults& results, const A11yResultsOpponent& opponent) {
    std::string name;
    if (opponent.tag[0] != '\0') {
        DecodedText tag = decode_shift_jis(opponent.tag);
        if (tag.unknown_glyphs > 0 || tag.malformed) {
            log_once("[a11y] results: name tag \"" + tag.text + "\"" +
                     (tag.unknown_glyphs > 0 ? ", unknown characters left out" : "") +
                     (tag.malformed ? ", malformed" : ""));
        }
        name = spoken_text(tag.text);
    }
    if (name.empty()) {
        name = port_name(opponent.port);
    }
    if (has_port(results, opponent.port)) {
        std::string character = character_word(results, opponent.port);
        if (!character.empty()) {
            name += ' ' + character;
        }
    }
    if (opponent.port == results_you(results)) {
        add_part(name, kYou);
    }
    return name;
}

std::vector<ResultsRow> ResultsSpeech::rows(const A11yResults& results, int port, int page) {
    std::vector<ResultsRow> out;
    const A11yResultsPlayer& player = results.players[port];
    /* A label from the game's text, or the fork's word where it has none. */
    auto label = [this](const std::uint8_t* text, std::string_view fallback) {
        std::string words = game_words(text, std::string(fallback));
        if (words.empty()) {
            log_once("[a11y] results: no game text for \"" + std::string(fallback) + "\"");
            return std::string(fallback);
        }
        return words;
    };
    auto value_row = [](std::string name, int value) {
        return ResultsRow{std::move(name), std::to_string(value), true};
    };
    switch (page) {
    case A11Y_RESULTS_PAGE_KOS: {
        /* Headings are rows of their own, as on screen; each other player is
         * named as the row shows them, by their tag or their port, with the
         * character. */
        int count = std::min(player.opponent_count, A11Y_RESULTS_PORTS - 1);
        out.push_back(ResultsRow{label(results.kos_heading, "KOs"), "", false});
        for (int i = 0; i < count; i++) {
            out.push_back(
                value_row(opponent_name(results, player.opponents[i]), player.opponents[i].kos));
        }
        out.push_back(ResultsRow{label(results.falls_heading, "Falls"), "", false});
        for (int i = 0; i < count; i++) {
            out.push_back(
                value_row(opponent_name(results, player.opponents[i]), player.opponents[i].falls));
        }
        out.push_back(value_row(label(results.sds_heading, "SDs"), player.page_sds));
        break;
    }
    case A11Y_RESULTS_PAGE_STATS:
        for (int i = 0; i < A11Y_RESULTS_STAT_COUNT; i++) {
            /* A label drawn over two rows is one label; the value row's own
             * string is empty for the others. */
            std::string name;
            for (const std::uint8_t* part : results.stat_labels[i]) {
                std::string words = game_words(part, "stat " + std::to_string(i + 1));
                if (!words.empty()) {
                    name += name.empty() ? words : ' ' + words;
                }
            }
            if (name.empty()) {
                log_once("[a11y] results: stat " + std::to_string(i + 1) + " has no label");
                name = "Stat " + std::to_string(i + 1);
            }
            int value = player.stats[i];
            out.push_back(ResultsRow{
                stat_label(name), value < 0 ? std::string(kNoValue) : std::to_string(value), true});
        }
        break;
    case A11Y_RESULTS_PAGE_BONUSES:
        for (int i = 0; i < std::min(player.bonus_count, int{A11Y_RESULTS_MAX_BONUSES}); i++) {
            const A11yResultsBonus& bonus = player.bonuses[i];
            out.push_back(
                value_row(label(bonus.name, "Bonus " + std::to_string(bonus.kind)), bonus.points));
        }
        break;
    default:
        break;
    }
    return out;
}

std::string ResultsSpeech::game_words(const std::uint8_t* text, const std::string& what) {
    if (text == nullptr) {
        return "";
    }
    DecodedText decoded = decode_game_text(text, m_text);
    if (decoded.unknown_glyphs > 0 || decoded.malformed) {
        log_once("[a11y] results: " + what + " \"" + one_line(decoded.text) + "\"" +
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
