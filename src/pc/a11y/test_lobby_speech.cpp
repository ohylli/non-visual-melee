/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Lobby speech against hand-written snapshots of the LAN lobby, over a fake
 * screen reader bridge. Links lobby_speech.cpp and speech.cpp only, so this
 * file supplies the pc_log_line that main.c normally provides. The status
 * lines are the base port's own (lobbyFillView in
 * src/melee/gm/gmonlinemode.c). */
#include "lobby_speech.hpp"
#include "scene_speech.hpp"
#include "speech.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Output {
    std::string text;
    bool interrupt;

    bool operator==(const Output&) const = default;
};

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    explicit FakeBridge(std::vector<Output>& outputs) : m_outputs(outputs) {}

    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view utf8, bool interrupt) override {
        m_outputs.push_back(Output{std::string(utf8), interrupt});
        return "";
    }

private:
    std::vector<Output>& m_outputs;
};

constexpr const char* kName = "DESKTOP-FUG0K6B";

struct Fixture {
    std::vector<Output> outputs;
    a11y::Speech speech{a11y::Config{true, false}, std::make_unique<FakeBridge>(outputs)};
    a11y::LobbySpeech lobby{speech};

    explicit Fixture(a11y::LobbyArrival arrival = a11y::LobbyArrival::from_menu) {
        speech.init();
        lobby.entered(arrival);
    }

    /* Hands the snapshot over and returns what it said, clearing it. */
    std::vector<Output> frame(const A11yLobbyState& state) {
        lobby.frame(state);
        std::vector<Output> said = std::move(outputs);
        outputs.clear();
        return said;
    }
};

A11yLobbyState lobby(A11yLobbyPhase phase, const char* message) {
    A11yLobbyState state{};
    state.lan = true;
    state.phase = phase;
    std::snprintf(state.message, sizeof(state.message), "%s", message);
    return state;
}

A11yLobbyState searching() {
    return lobby(A11Y_LOBBY_SEARCHING, "LAN: searching...");
}

A11yLobbyState with_row(A11yLobbyState state, const char* name, bool other_version = false) {
    A11yLobbyRow& row = state.rows[state.row_count++];
    std::snprintf(row.name, sizeof(row.name), "%s", name);
    row.other_version = other_version;
    return state;
}

A11yLobbyState found(const char* name) {
    return with_row(lobby(A11Y_LOBBY_FOUND, "2 players found - press START"), name);
}

std::vector<Output> queued(const char* text) {
    return {{text, false}};
}

void opening_while_searching() {
    Fixture f;
    assert((f.frame(searching()) ==
            std::vector<Output>{{"LAN play. Searching for players. B to go back.", true}}));
    assert(f.frame(searching()).empty());
}

void opening_names_machines_already_listed() {
    Fixture f;
    assert((f.frame(found(kName)) ==
            std::vector<Output>{
                {"LAN play. Found DESKTOP-FUG0K6B. Press Start to play. B to go back.", true}}));
}

void opening_with_only_another_version() {
    Fixture f;
    assert((f.frame(with_row(searching(), "OLDPC", true)) ==
            std::vector<Output>{{"LAN play. Found OLDPC, other version, cannot play. Searching "
                                 "for players. B to go back.",
                true}}));
}

void machine_appearing_and_leaving() {
    Fixture f;
    f.frame(searching());
    assert(f.frame(found(kName)) == queued("Found DESKTOP-FUG0K6B. Press Start to play."));
    A11yLobbyState two = with_row(found(kName), "LAPTOP");
    std::snprintf(two.message, sizeof(two.message), "3 players found - press START");
    /* Only the first machine to appear adds the way to start. */
    assert(f.frame(two) == queued("Found LAPTOP."));
    assert(f.frame(found("LAPTOP")) == queued("DESKTOP-FUG0K6B left."));
    assert(f.frame(searching()) == queued("LAPTOP left. Searching for players."));
}

void rows_reordering_is_silent() {
    Fixture f;
    f.frame(with_row(found("A"), "B"));
    assert(f.frame(with_row(found("B"), "A")).empty());
}

void two_machines_of_one_name() {
    /* Two copies on one computer share its hostname. */
    Fixture f;
    f.frame(found(kName));
    assert(f.frame(with_row(found(kName), kName)) == queued("Found DESKTOP-FUG0K6B."));
    assert(f.frame(found(kName)) == queued("DESKTOP-FUG0K6B left."));
}

void another_version_appearing() {
    Fixture f;
    f.frame(searching());
    assert(f.frame(with_row(searching(), "OLDPC", true)) ==
           queued("Found OLDPC, other version, cannot play."));
    /* A playable one after it still adds the way to start. */
    A11yLobbyState both = with_row(found(kName), "OLDPC", true);
    assert(f.frame(both) == queued("Found DESKTOP-FUG0K6B. Press Start to play."));
}

void network_hiding_players() {
    Fixture f;
    f.frame(searching());
    assert(f.frame(lobby(
               A11Y_LOBBY_SEARCHING, "This network hides other players - try DIRECT CONNECT")) ==
           queued("This network hides other players. Try Direct connect instead."));
}

void start_through_to_starting() {
    Fixture f;
    f.frame(found(kName));
    A11yLobbyState waiting =
        with_row(lobby(A11Y_LOBBY_CONNECTING, "Ready - waiting for host..."), kName);
    assert(f.frame(waiting) == queued("Waiting for host."));
    A11yLobbyState connecting = with_row(lobby(A11Y_LOBBY_CONNECTING, "Connecting..."), kName);
    assert(f.frame(connecting) == queued("Connecting."));
    A11yLobbyState starting = with_row(lobby(A11Y_LOBBY_STARTING, "Starting..."), kName);
    assert(f.frame(starting) == queued("Starting."));
    assert(f.frame(starting).empty());
}

void failure_says_the_reason() {
    Fixture f;
    f.frame(found(kName));
    A11yLobbyState failed =
        with_row(lobby(A11Y_LOBBY_FAILED, "Failed: peer left - Peer left - START: retry"), kName);
    assert(f.frame(failed) == queued("Failed: peer left - Peer left. Start to retry."));
    assert(f.frame(failed).empty());
    /* The retry hint goes with the last machine, which is said by itself. */
    assert(f.frame(lobby(A11Y_LOBBY_FAILED, "Failed: peer left - Peer left")) ==
           queued("DESKTOP-FUG0K6B left."));
    assert(f.frame(lobby(A11Y_LOBBY_FAILED, "Failed: timeout")) == queued("Failed: timeout."));
}

void status_lines_sorted() {
    using Kind = a11y::LobbyStatus::Kind;
    assert(a11y::lobby_status(searching()).kind == Kind::searching);
    assert(a11y::lobby_status(lobby(A11Y_LOBBY_SEARCHING, "LAN: searching... - Lobby full")).kind ==
           Kind::searching);
    a11y::LobbyStatus failed =
        a11y::lobby_status(lobby(A11Y_LOBBY_FAILED, "Failed: connect failed - START: retry"));
    assert(failed.kind == Kind::failed && failed.reason == "connect failed" && failed.retry);
}

void back_from_character_select() {
    Fixture f(a11y::LobbyArrival::backed_out);
    assert((f.frame(searching()) ==
            std::vector<Output>{{"Back to LAN play. Searching for players. B to go back.", true}}));
}

void connection_lost() {
    Fixture f(a11y::LobbyArrival::connection_lost);
    assert((f.frame(searching()) ==
            std::vector<Output>{
                {"Connection lost. LAN play. Searching for players. B to go back.", true}}));
}

void each_entry_opens_again() {
    Fixture f;
    f.frame(found(kName));
    f.lobby.entered(a11y::LobbyArrival::backed_out);
    assert((f.frame(found(kName)) ==
            std::vector<Output>{{"Back to LAN play. Found DESKTOP-FUG0K6B. Press Start to play. "
                                 "B to go back.",
                true}}));
}

void other_layouts_are_silent() {
    Fixture f;
    A11yLobbyState direct = searching();
    direct.lan = false;
    assert(f.frame(direct).empty());
    /* And the opening still waits for LAN play's list. */
    assert(f.frame(searching()).size() == 1);
}

void frames_not_handed_over_repeat_nothing() {
    /* While rollback re-runs frames the hook passes none on; the next frame
     * is compared with the last one spoken, so nothing is said twice. */
    Fixture f;
    f.frame(searching());
    f.frame(found(kName));
    assert(f.frame(found(kName)).empty());
}

void arrival_from_the_scene_left() {
    using a11y::LobbyArrival;
    using a11y::SceneKind;
    auto scene = [](SceneKind kind) { return static_cast<int>(kind); };
    assert(a11y::lobby_arrival(scene(SceneKind::GS_MENU), A11Y_CSS_STAYING) ==
           LobbyArrival::from_menu);
    assert(a11y::lobby_arrival(-1, A11Y_CSS_STAYING) == LobbyArrival::from_menu);
    assert(
        a11y::lobby_arrival(scene(SceneKind::GS_CSS), A11Y_CSS_BACK) == LobbyArrival::backed_out);
    assert(a11y::lobby_arrival(scene(SceneKind::GS_CSS), A11Y_CSS_STAYING) ==
           LobbyArrival::connection_lost);
    assert(a11y::lobby_arrival(scene(SceneKind::GS_VS), A11Y_CSS_STAYING) ==
           LobbyArrival::connection_lost);
    assert(a11y::lobby_arrival(scene(SceneKind::GS_SSS), A11Y_CSS_BACK) ==
           LobbyArrival::connection_lost);
    assert(a11y::lobby_arrival(scene(SceneKind::GS_RESULTS), A11Y_CSS_STAYING) ==
           LobbyArrival::connection_lost);
}

}  // namespace

extern "C" void pc_log_line(const char* fmt, ...) {
    (void)fmt;
}

int main() {
    opening_while_searching();
    opening_names_machines_already_listed();
    opening_with_only_another_version();
    machine_appearing_and_leaving();
    rows_reordering_is_silent();
    two_machines_of_one_name();
    another_version_appearing();
    network_hiding_players();
    start_through_to_starting();
    failure_says_the_reason();
    status_lines_sorted();
    back_from_character_select();
    connection_lost();
    each_entry_opens_again();
    other_layouts_are_silent();
    frames_not_handed_over_repeat_nothing();
    arrival_from_the_scene_left();
    std::cout << "lobby_speech: all tests passed\n";
    return 0;
}
