/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Direct Connect's speech against hand-written snapshots of the hub and the
 * code keyboard, over a fake screen reader bridge. Links direct_speech.cpp
 * and speech.cpp only, so this file supplies the pc_log_line that main.c
 * normally provides. The lines are the base port's
 * own (directEntryFrame in src/melee/gm/gmonlinemode.c). */
#include "direct_speech.hpp"
#include "speech.hpp"
#include <cassert>
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

struct Fixture {
    std::vector<Output> outputs;
    a11y::Speech speech{a11y::Config{true, false}, std::make_unique<FakeBridge>(outputs)};
    a11y::DirectSpeech direct{speech};

    explicit Fixture(a11y::LobbyArrival arrival = a11y::LobbyArrival::from_menu) {
        speech.init();
        direct.entered(arrival);
    }

    /* Hands the snapshot over and returns what it said, clearing it. */
    std::vector<Output> frame(const A11yLobbyState& state) {
        direct.frame(state);
        std::vector<Output> said = std::move(outputs);
        outputs.clear();
        return said;
    }
};

constexpr const char* kPickLine = "Pick who to play. Both of you can call each other.";
constexpr const char* kHubHint = "A: select   Y: copy my code   B: back";
constexpr const char* kTypeLine = "Type the 8 characters after the #";
constexpr const char* kKeysHint = "A: type   B: delete   START: call";
constexpr const char* kKeys = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
constexpr const char* kEnter = "Enter a friend's code";
constexpr const char* kWait = "Wait for a friend to call me";
/* A recent opponent's row, its code padded to 22 characters. */
constexpr const char* kRecent = "RYAN#K3X-Q2M-7A        played 5m ago";
constexpr const char* kPaste = "Call SAM#ABC-DEF-GH  from clipboard";

A11yLobbyState direct_screen(A11yDirectScreen screen, A11yLobbyPhase phase) {
    A11yLobbyState state{};
    state.direct_connect = true;
    state.phase = phase;
    state.direct.screen = screen;
    std::snprintf(state.direct.title, sizeof(state.direct.title), "DIRECT CONNECT");
    std::snprintf(state.direct.subtitle, sizeof(state.direct.subtitle), "YOU RYAN#K3X-Q2M-7A");
    return state;
}

void set(char* out, size_t size, const char* text) {
    std::snprintf(out, size, "%s", text);
}

/* The hub with its rows, the cursor on the row numbered cursor. */
A11yLobbyState hub(std::vector<const char*> rows, int cursor = 0, const char* message = kPickLine) {
    A11yLobbyState state = direct_screen(A11Y_DIRECT_HUB, A11Y_LOBBY_FOUND);
    for (const char* row : rows) {
        set(state.direct.rows[state.direct.row_count++], sizeof(state.direct.rows[0]), row);
    }
    state.direct.cursor = cursor;
    set(state.message, sizeof(state.message), message);
    set(state.direct.hint, sizeof(state.direct.hint), kHubHint);
    return state;
}

A11yLobbyState fresh_hub(int cursor = 0) {
    return hub({kEnter, kWait}, cursor);
}

/* The code keyboard with typed already typed, the cursor on the key
 * numbered key. */
A11yLobbyState keys(const char* typed, int key = 0, const char* message = kTypeLine) {
    A11yLobbyState state = direct_screen(A11Y_DIRECT_KEYS, A11Y_LOBBY_FOUND);
    std::string slots(typed);
    slots.resize(8, '_');
    std::snprintf(state.direct.rows[0], sizeof(state.direct.rows[0]),
        "Friend's code   # %.3s-%.3s-%.2s", slots.c_str(), slots.c_str() + 3, slots.c_str() + 6);
    state.direct.row_count = 1;
    set(state.direct.keys, sizeof(state.direct.keys), kKeys);
    state.direct.key_cursor = key;
    set(state.message, sizeof(state.message), message);
    set(state.direct.hint, sizeof(state.direct.hint), kKeysHint);
    return state;
}

int key_of(char c) {
    return static_cast<int>(std::string(kKeys).find(c));
}

std::vector<Output> said(const char* text) {
    return {{text, true}};
}

void hub_opening() {
    Fixture f;
    assert(f.frame(fresh_hub()) ==
           said("DIRECT CONNECT. YOU RYAN#K3X-Q2M-7A. Enter a friend's code. Pick who to play. "
                "Both of you can call each other. A: select Y: copy my code B: back"));
    assert(f.frame(fresh_hub()).empty());
}

void hub_moves_say_the_row() {
    Fixture f;
    f.frame(fresh_hub());
    assert(f.frame(fresh_hub(1)) == said("Wait for a friend to call me"));
    assert(f.frame(fresh_hub(0)) == said("Enter a friend's code"));
}

void padding_spaces_are_collapsed() {
    Fixture f;
    f.frame(hub({kRecent, kEnter, kWait}));
    assert(f.frame(hub({kRecent, kEnter, kWait}, 1)) == said(kEnter));
    assert(f.frame(hub({kRecent, kEnter, kWait}, 0)) == said("RYAN#K3X-Q2M-7A played 5m ago"));
}

void rows_changing_without_a_press() {
    Fixture f;
    f.frame(fresh_hub(1));
    /* A code copied while the hub is open: the game puts the cursor on the
     * new top row. */
    assert(f.frame(hub({kPaste, kEnter, kWait})) == said("Call SAM#ABC-DEF-GH from clipboard"));
    /* Another code in its place, the cursor staying. */
    assert(f.frame(hub({"Call AL#XYZ-XYZ-XY  from clipboard", kEnter, kWait})) ==
           said("Call AL#XYZ-XYZ-XY from clipboard"));
    /* Gone again. */
    assert(f.frame(fresh_hub()) == said(kEnter));
}

void a_recent_opponent_ageing_is_silent() {
    Fixture f;
    f.frame(hub({kRecent, kEnter, kWait}));
    assert(f.frame(hub({"RYAN#K3X-Q2M-7A        played 6m ago", kEnter, kWait})).empty());
}

void hub_message_line() {
    Fixture f;
    f.frame(fresh_hub());
    assert(f.frame(hub({kEnter, kWait}, 0, "Your code is copied - send it to a friend")) ==
           said("Your code is copied - send it to a friend"));
    /* A move and the line on one frame: the row, then the line. */
    assert(f.frame(fresh_hub(1)) ==
           (std::vector<Output>{{"Wait for a friend to call me", true}, {kPickLine, false}}));
}

void keyboard_opening() {
    Fixture f;
    f.frame(fresh_hub());
    assert(f.frame(keys("")) ==
           said("Type the 8 characters after the #. A to Z, then 2 to 7, 8 to a row, edges "
                "wrap. A: type B: delete START: call. A"));
}

void keyboard_moves_say_the_key() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(keys(""));
    assert(f.frame(keys("", key_of('B'))) == said("B"));
    assert(f.frame(keys("", key_of('J'))) == said("J"));
    assert(f.frame(keys("", key_of('7'))) == said("7"));
}

void typing_and_deleting() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(keys("", key_of('K')));
    assert(f.frame(keys("K", key_of('K'))) == said("K, 1 of 8"));
    f.frame(keys("K", key_of('3')));
    assert(f.frame(keys("K3", key_of('3'))) == said("3, 2 of 8"));
    /* A move and a press on one frame: the character typed names the key. */
    assert(f.frame(keys("K3X", key_of('X'))) == said("X, 3 of 8"));
    assert(f.frame(keys("K3", key_of('X'))) == said("Deleted X"));
    assert(f.frame(keys("K", key_of('X'))) == said("Deleted 3"));
}

void keyboard_error() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(keys("K3X"));
    const char* error = "A code has 8 characters after the #";
    assert(f.frame(keys("K3X", 0, error)) == said(error));
    /* The next character clears it, which the character's own words cover. */
    assert(f.frame(keys("K3XA", 0)) == said("A, 4 of 8"));
}

void back_to_the_hub_without_a_session() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(keys(""));
    assert(f.frame(fresh_hub()) == said("DIRECT CONNECT. Enter a friend's code"));
}

void returns_from_a_session() {
    using a11y::LobbyArrival;
    Fixture back(LobbyArrival::backed_out);
    A11yLobbyState again = hub({kRecent, kEnter, kWait});
    assert(back.frame(again) == said("Back to DIRECT CONNECT. RYAN#K3X-Q2M-7A played 5m ago"));
    Fixture lost(LobbyArrival::connection_lost);
    assert(lost.frame(again) ==
           said("Connection lost. DIRECT CONNECT. RYAN#K3X-Q2M-7A played 5m ago"));
}

void each_entry_opens_again() {
    Fixture f;
    f.frame(fresh_hub());
    f.direct.entered(a11y::LobbyArrival::from_menu);
    assert(f.frame(fresh_hub()).size() == 1);
}

void the_call_is_not_read_here() {
    Fixture f;
    f.frame(fresh_hub(1));
    A11yLobbyState call = direct_screen(A11Y_DIRECT_CALL, A11Y_LOBBY_SEARCHING);
    set(call.message, sizeof(call.message), "Joining the online network...  0:00");
    assert(f.frame(call).empty());
}

void other_lobbies_are_silent() {
    Fixture f;
    A11yLobbyState lan{};
    lan.lan = true;
    assert(f.frame(lan).empty());
    /* And the opening still waits for Direct Connect's view. */
    assert(f.frame(fresh_hub()).size() == 1);
}

void frames_not_handed_over_repeat_nothing() {
    /* While rollback re-runs frames the hook passes none on; the next frame
     * is compared with the last one spoken, so nothing is said twice. */
    Fixture f;
    f.frame(fresh_hub());
    f.frame(fresh_hub(1));
    assert(f.frame(fresh_hub(1)).empty());
}

}  // namespace

extern "C" void pc_log_line(const char* fmt, ...) {
    (void)fmt;
}

int main() {
    hub_opening();
    hub_moves_say_the_row();
    padding_spaces_are_collapsed();
    rows_changing_without_a_press();
    a_recent_opponent_ageing_is_silent();
    hub_message_line();
    keyboard_opening();
    keyboard_moves_say_the_key();
    typing_and_deleting();
    keyboard_error();
    back_to_the_hub_without_a_session();
    returns_from_a_session();
    each_entry_opens_again();
    the_call_is_not_read_here();
    other_lobbies_are_silent();
    frames_not_handed_over_repeat_nothing();
    std::cout << "direct_speech: all tests passed\n";
    return 0;
}
