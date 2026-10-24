/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Direct Connect's speech against hand-written snapshots of the hub, the
 * code keyboard and the call, over a fake screen reader bridge. Links
 * direct_speech.cpp, wait_status.cpp and speech.cpp only, so this file
 * supplies the pc_log_line that main.c normally provides. The lines are the
 * base port's own (directEntryFrame and the internet branch of
 * gm_Scene_OnlineLobby_OnFrame in src/melee/gm/gmonlinemode.c, and
 * src/pc/net_match.c). */
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

constexpr const char* kCallHint = "Y: copy my code   B: stop";

/* The call's screen with its status line, ms into the call; the hint shows
 * while it searches. */
A11yLobbyState call(const char* message, unsigned ms, A11yLobbyPhase phase = A11Y_LOBBY_SEARCHING) {
    A11yLobbyState state = direct_screen(A11Y_DIRECT_CALL, phase);
    set(state.message, sizeof(state.message), message);
    state.direct.call_ms = ms;
    if (phase == A11Y_LOBBY_SEARCHING) {
        set(state.direct.hint, sizeof(state.direct.hint), kCallHint);
    }
    return state;
}

/* "Calling SAM#ABC-DEF-GH  0:12" at seconds, as the base port prints it,
 * with its change at 0:45. */
A11yLobbyState calling(unsigned seconds) {
    char line[96];
    std::snprintf(line, sizeof(line),
        seconds < 45 ? "Calling SAM#ABC-DEF-GH  %u:%02u" :
                       "SAM#ABC-DEF-GH isn't answering yet. Still calling  %u:%02u",
        seconds / 60, seconds % 60);
    return call(line, seconds * 1000 + 300);
}

std::vector<Output> queued(const char* text) {
    return {{text, false}};
}

void call_opening() {
    Fixture f;
    f.frame(fresh_hub());
    assert(f.frame(call("Joining the online network...  0:00", 16)) ==
           said("Joining the online network... 0:00. Y: copy my code B: stop"));
}

void the_clock_alone_is_no_change() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(call("Joining the online network...  0:00", 16));
    assert(f.frame(call("Joining the online network...  0:01", 1016)).empty());
    assert(f.frame(call("Joining the online network...  0:02", 2016)).empty());
    /* The DHT reached: a new line, queued behind the opening. */
    assert(f.frame(calling(3)) == queued("Calling SAM#ABC-DEF-GH 0:03"));
    assert(f.frame(calling(4)).empty());
}

void reminders_on_a_rhythm() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(calling(0));
    assert(f.frame(calling(14)).empty());
    assert(f.frame(calling(15)) == queued("Calling SAM#ABC-DEF-GH 0:15"));
    assert(f.frame(calling(16)).empty());
    assert(f.frame(calling(29)).empty());
    assert(f.frame(calling(30)) == queued("Calling SAM#ABC-DEF-GH 0:30"));
    /* The base port's own change, with no reminder of its own. */
    assert(f.frame(calling(44)).empty());
    assert(
        f.frame(calling(45)) == queued("SAM#ABC-DEF-GH isn't answering yet. Still calling 0:45"));
    assert(f.frame(calling(60)).empty());
    assert(f.frame(calling(74)).empty());
    assert(
        f.frame(calling(75)) == queued("SAM#ABC-DEF-GH isn't answering yet. Still calling 1:15"));
    assert(f.frame(calling(104)).empty());
    assert(
        f.frame(calling(105)) == queued("SAM#ABC-DEF-GH isn't answering yet. Still calling 1:45"));
    /* A frame skipped past a moment still reminds, once. */
    assert(
        f.frame(calling(137)) == queued("SAM#ABC-DEF-GH isn't answering yet. Still calling 2:17"));
    assert(f.frame(calling(138)).empty());
}

void a_reminder_follows_the_line_shown() {
    /* The clock drawn can lag the call's own by a frame; the reminder waits
     * for the line to show its moment. */
    Fixture f;
    f.frame(fresh_hub());
    f.frame(call("Waiting for a friend to call you  0:00", 0));
    assert(f.frame(call("Waiting for a friend to call you  0:14", 15004)).empty());
    assert(f.frame(call("Waiting for a friend to call you  0:15", 15020)) ==
           queued("Waiting for a friend to call you 0:15"));
}

void a_line_without_a_clock_repeats_at_the_moments() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(calling(0));
    f.frame(calling(16));
    const char* trying = "Found them but couldn't connect. Still trying...";
    assert(f.frame(call(trying, 20000)) == queued(trying));
    assert(f.frame(call(trying, 29990)).empty());
    assert(f.frame(call(trying, 30010)) == queued(trying));
    assert(f.frame(call(trying, 74990)).empty());
    assert(f.frame(call(trying, 75010)) == queued(trying));
    /* Back to calling: the line changed. */
    assert(
        f.frame(calling(80)) == queued("SAM#ABC-DEF-GH isn't answering yet. Still calling 1:20"));
}

void connecting_and_starting() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(calling(0));
    /* Connecting shows the base port's line for a call with no reason. */
    const char* line = "Searching for an opponent...";
    assert(f.frame(call(line, 0, A11Y_LOBBY_CONNECTING)) == queued(line));
    /* No clock and no wait: no reminders, however long it takes. */
    for (int i = 0; i < 3; i++) {
        assert(f.frame(call(line, 0, A11Y_LOBBY_CONNECTING)).empty());
    }
    A11yLobbyState starting = call(line, 0, A11Y_LOBBY_STARTING);
    set(starting.direct.countdown, sizeof(starting.direct.countdown), "Starting in 1...");
    assert(f.frame(starting) == queued("Starting in 1..."));
    assert(f.frame(starting).empty());

    /* Both on one frame: in the order the call reaches them. */
    Fixture g;
    g.frame(fresh_hub());
    g.frame(calling(0));
    assert(g.frame(starting) == (std::vector<Output>{{line, false}, {"Starting in 1...", false}}));
}

void a_failure_and_back_to_the_hub() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(calling(0));
    f.frame(calling(12));
    const char* failed = "Peer left - START: search";
    assert(f.frame(call(failed, 0, A11Y_LOBBY_FAILED)) == queued(failed));
    assert(f.frame(call(failed, 0, A11Y_LOBBY_FAILED)).empty());
    /* Start goes back to the hub without a session. */
    assert(f.frame(hub({kRecent, kEnter, kWait})) ==
           said("DIRECT CONNECT. RYAN#K3X-Q2M-7A played 5m ago"));
}

void a_call_that_fails_at_once() {
    /* Start on the keyboard with your own code: the call's first frame has
     * failed and shows no buttons of its own. */
    Fixture f;
    f.frame(fresh_hub());
    f.frame(keys("K3XQ2M7A"));
    assert(f.frame(call("that is your own code", 0, A11Y_LOBBY_FAILED)) ==
           said("that is your own code"));
}

void the_opening_leaves_out_a_hint_the_call_does_not_own() {
    /* The frame a hub row starts a call that fails at once still carries
     * the hub's buttons. */
    Fixture f;
    f.frame(fresh_hub(1));
    A11yLobbyState failed = call("DHT unavailable", 0, A11Y_LOBBY_FAILED);
    set(failed.direct.hint, sizeof(failed.direct.hint), kHubHint);
    assert(f.frame(failed) == said("DHT unavailable"));
}

void each_call_starts_its_own_rhythm() {
    Fixture f;
    f.frame(fresh_hub());
    f.frame(calling(0));
    f.frame(calling(40));
    f.frame(call("Peer left - START: search", 0, A11Y_LOBBY_FAILED));
    f.frame(fresh_hub());
    assert(f.frame(calling(0)) == said("Calling SAM#ABC-DEF-GH 0:00. Y: copy my code B: stop"));
    assert(f.frame(calling(15)) == queued("Calling SAM#ABC-DEF-GH 0:15"));
}

void a_moment_is_said_once() {
    /* While rollback re-runs frames the hook hands none over, and the next
     * frame handed over may show the same moment again. */
    Fixture f;
    f.frame(fresh_hub());
    f.frame(calling(0));
    assert(f.frame(calling(15)).size() == 1);
    assert(f.frame(calling(15)).empty());
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
    call_opening();
    the_clock_alone_is_no_change();
    reminders_on_a_rhythm();
    a_reminder_follows_the_line_shown();
    a_line_without_a_clock_repeats_at_the_moments();
    connecting_and_starting();
    a_failure_and_back_to_the_hub();
    a_call_that_fails_at_once();
    the_opening_leaves_out_a_hint_the_call_does_not_own();
    each_call_starts_its_own_rhythm();
    a_moment_is_said_once();
    other_lobbies_are_silent();
    frames_not_handed_over_repeat_nothing();
    std::cout << "direct_speech: all tests passed\n";
    return 0;
}
