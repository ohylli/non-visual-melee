/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "lobby_speech.hpp"
#include "scene_speech.hpp"
#include "speech.hpp"
#include <algorithm>
#include <string_view>

namespace a11y {
namespace {

/* The base port's status lines (lobbyFillView in src/melee/gm/gmonlinemode.c)
 * by the start or end that tells them apart where the phase alone does not. */
constexpr std::string_view kWaitingLine = "Ready - waiting for host";
constexpr std::string_view kHiddenLine = "This network hides other players";
constexpr std::string_view kFailedPrefix = "Failed: ";
constexpr std::string_view kRetrySuffix = " - START: retry";

/* Wording is a first proposal for the play test to judge. */
constexpr std::string_view kLanPlay = "LAN play.";
constexpr std::string_view kBackToLanPlay = "Back to LAN play.";
constexpr std::string_view kConnectionLost = "Connection lost.";
constexpr std::string_view kWayBack = "B to go back.";
constexpr std::string_view kPressStart = "Press Start to play.";

bool starts_with(std::string_view text, std::string_view start) {
    return text.substr(0, start.size()) == start;
}

bool ends_with(std::string_view text, std::string_view end) {
    return text.size() >= end.size() && text.substr(text.size() - end.size()) == end;
}

/* The sentence a status is said with; empty for found, which the machines'
 * own sentences say. */
std::string status_sentence(const LobbyStatus& status) {
    switch (status.kind) {
    case LobbyStatus::Kind::searching:
        return "Searching for players.";
    case LobbyStatus::Kind::hidden:
        return "This network hides other players. Try Direct connect instead.";
    case LobbyStatus::Kind::found:
        return "";
    case LobbyStatus::Kind::waiting_for_host:
        return "Waiting for host.";
    case LobbyStatus::Kind::connecting:
        return "Connecting.";
    case LobbyStatus::Kind::starting:
        return "Starting.";
    case LobbyStatus::Kind::failed:
        return "Failed: " + status.reason + "." + (status.retry ? " Start to retry." : "");
    }
    return "";
}

void add(std::string& text, std::string_view sentence) {
    if (sentence.empty()) {
        return;
    }
    if (!text.empty()) {
        text += ' ';
    }
    text += sentence;
}

std::string found_sentence(std::string_view name, bool other_version) {
    std::string out = "Found ";
    out += name;
    out += other_version ? ", other version, cannot play." : ".";
    return out;
}

bool any_playable(const auto& rows) {
    return std::any_of(
        rows.begin(), rows.end(), [](const auto& row) { return !row.other_version; });
}

/* The rows of from that to lacks, counting each row of a repeated name once:
 * two copies on one machine share its hostname. */
template <typename Row>
std::vector<Row> missing_from(const std::vector<Row>& from, std::vector<Row> to) {
    std::vector<Row> missing;
    for (const Row& row : from) {
        auto match = std::find(to.begin(), to.end(), row);
        if (match == to.end()) {
            missing.push_back(row);
        } else {
            to.erase(match);
        }
    }
    return missing;
}

}  // namespace

LobbyArrival lobby_arrival(int previous_scene, int css_exit) {
    switch (static_cast<SceneKind>(previous_scene)) {
    case SceneKind::GS_CSS:
        return css_exit == A11Y_CSS_BACK ? LobbyArrival::backed_out : LobbyArrival::connection_lost;
    case SceneKind::GS_SSS:
    case SceneKind::GS_VS:
    case SceneKind::GS_SUDDEN_DEATH:
    case SceneKind::GS_RESULTS:
        return LobbyArrival::connection_lost;
    default:
        return LobbyArrival::from_menu;
    }
}

LobbyStatus lobby_status(const A11yLobbyState& state) {
    std::string_view message(state.message);
    LobbyStatus status;
    switch (state.phase) {
    case A11Y_LOBBY_SEARCHING:
        status.kind = starts_with(message, kHiddenLine) ? LobbyStatus::Kind::hidden :
                                                          LobbyStatus::Kind::searching;
        break;
    case A11Y_LOBBY_FOUND:
        status.kind = LobbyStatus::Kind::found;
        break;
    case A11Y_LOBBY_CONNECTING:
        status.kind = starts_with(message, kWaitingLine) ? LobbyStatus::Kind::waiting_for_host :
                                                           LobbyStatus::Kind::connecting;
        break;
    case A11Y_LOBBY_STARTING:
        status.kind = LobbyStatus::Kind::starting;
        break;
    case A11Y_LOBBY_FAILED:
        status.kind = LobbyStatus::Kind::failed;
        if (starts_with(message, kFailedPrefix)) {
            message.remove_prefix(kFailedPrefix.size());
        }
        status.retry = ends_with(message, kRetrySuffix);
        if (status.retry) {
            message.remove_suffix(kRetrySuffix.size());
        }
        status.reason = message;
        break;
    }
    return status;
}

void LobbySpeech::frame(const A11yLobbyState& state) {
    if (!state.lan) {
        return;
    }
    std::vector<Row> rows;
    for (int i = 0; i < state.row_count && i < A11Y_LOBBY_ROWS; i++) {
        rows.push_back(Row{state.rows[i].name, state.rows[i].other_version});
    }
    LobbyStatus status = lobby_status(state);
    if (!m_seen) {
        opening(rows, status);
        m_seen = true;
    } else {
        changes(rows, status);
    }
    m_rows = std::move(rows);
    m_status = std::move(status);
}

void LobbySpeech::opening(const std::vector<Row>& rows, const LobbyStatus& status) {
    std::string text;
    switch (m_arrival) {
    case LobbyArrival::from_menu:
        add(text, kLanPlay);
        break;
    case LobbyArrival::backed_out:
        add(text, kBackToLanPlay);
        break;
    case LobbyArrival::connection_lost:
        add(text, kConnectionLost);
        add(text, kLanPlay);
        break;
    }
    for (const Row& row : rows) {
        add(text, found_sentence(row.name, row.other_version));
    }
    if (status.kind == LobbyStatus::Kind::found && any_playable(rows)) {
        add(text, kPressStart);
    }
    add(text, status_sentence(status));
    add(text, kWayBack);
    m_speech.announce(text, Mode::interrupt);
}

void LobbySpeech::changes(const std::vector<Row>& rows, const LobbyStatus& status) {
    std::string text;
    for (const Row& row : missing_from(m_rows, rows)) {
        add(text, row.name + " left.");
    }
    for (const Row& row : missing_from(rows, m_rows)) {
        add(text, found_sentence(row.name, row.other_version));
    }
    if (status.kind == LobbyStatus::Kind::found && any_playable(rows) && !any_playable(m_rows)) {
        add(text, kPressStart);
    }
    /* A failure's retry hint comes and goes with the machines listed, which
     * are said by themselves. */
    bool changed = status.kind != m_status.kind || status.reason != m_status.reason;
    if (changed) {
        add(text, status_sentence(status));
    }
    if (!text.empty()) {
        m_speech.announce(text, Mode::queue);
    }
}

}  // namespace a11y
