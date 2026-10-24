/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Direct Connect's speech: reads the DIRECT CONNECT hub and the code keyboard
 * aloud from a snapshot of the lobby's view taken once a frame
 * (.scratch/direct-connect/spec.md). Every line is said as the screen shows
 * it, runs of spaces collapsed; only what the screen gives by sight alone is
 * in the fork's words: the keyboard's layout, the hovered key and the count
 * typed.
 *
 * The hub opens with its title, your code, the hovered row, the line under
 * the list and the buttons, and says each hovered row after that, whether a
 * press or a code on the clipboard moved it, and each new line under the
 * list. The keyboard opens with its line, its layout, the buttons and the
 * hovered key, then says each key moved to, each character typed with the
 * count, each one deleted and the error. The call opens with its status
 * line and its buttons, then says each new status line, its clock left out of
 * the comparison, with reminders while it waits (wait_status.hpp), and the
 * countdown as it starts. Back on the hub from the keyboard or a failed call
 * it says the title and the hovered row. It compares each snapshot with the
 * previous one and touches no game state itself. */
#pragma once
#include "game_access.h"
#include "lobby_speech.hpp"
#include "wait_status.hpp"
#include <string>
#include <vector>

namespace a11y {

class Speech;

class DirectSpeech {
public:
    /* speech must outlive this. */
    explicit DirectSpeech(Speech& speech) : m_speech(speech) {}

    /* The lobby scene was entered: the next snapshot is spoken as the
     * opening, after the words for the arrival. A return from a session says
     * the title and the hovered row alone: the player heard the rest on the
     * way in. */
    void entered(LobbyArrival arrival) {
        m_arrival = arrival;
        m_seen = false;
    }

    /* The lobby's state this frame. Speaks what changed since the last one;
     * nothing outside Direct Connect's view. */
    void frame(const A11yLobbyState& state);

private:
    /* One frame's view by what is spoken from it, spaces collapsed. */
    struct Shown {
        A11yDirectScreen screen = A11Y_DIRECT_HUB;
        std::string title;
        std::string subtitle;
        std::vector<std::string> rows;
        int cursor = 0;
        std::string hovered_key;
        /* KEYS: the characters typed, and how many the code has. */
        std::string typed;
        int code_length = 0;
        std::string layout;
        std::string message;
        std::string hint;
        /* CALL: the countdown line as it starts, and the wait's clock, 0
         * when it is not waiting. */
        std::string countdown;
        unsigned wait_ms = 0;
    };

    static Shown shown(const A11yLobbyState& state);
    void opening(const Shown& now);
    void screen_changed(const Shown& now);
    void hub_changes(const Shown& now);
    void keys_changes(const Shown& now);
    std::string keys_opening(const Shown& now) const;
    void call_opening(const Shown& now);
    void call_changes(const Shown& now);

    Speech& m_speech;
    LobbyArrival m_arrival = LobbyArrival::from_menu;
    bool m_seen = false;
    Shown m_last;
    WaitStatus m_status;
};

}  // namespace a11y
