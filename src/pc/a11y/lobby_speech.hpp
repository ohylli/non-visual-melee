/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Lobby speech: reads the LAN lobby aloud from a snapshot of its view taken
 * once a frame (.scratch/lan-play/spec.md, "Lobby speech"). The opening says
 * where the player is, the machines already listed or the search, and the way
 * back; then each machine appearing or leaving, and each change of status:
 * waiting for the host, connecting, starting, a network that hides players,
 * and a failure with the base port's reason. Ping, the host badge and the
 * countdown stay unsaid. Both machines speak the same, whichever pressed
 * Start. It compares each snapshot with the previous one and touches no game
 * state itself. */
#pragma once
#include "game_access.h"
#include <string>
#include <vector>

namespace a11y {

class Speech;

/* How the player came to the lobby, said before the opening. */
enum class LobbyArrival {
    /* From the Online screen. */
    from_menu,
    /* Someone backed out of character select: both machines return. */
    backed_out,
    /* The other machine was lost, anywhere in the session. */
    connection_lost,
};

/* The arrival a lobby scene makes after previous_scene (a GameSceneKind, or
 * -1 for none), given the pending exit character select last showed (an
 * A11yCssExit, read when previous_scene is character select). The lobby
 * itself cannot tell: every session ends with the same peer status by the
 * time it opens, so this is read from the scene the session left. Character
 * select goes back only when it was asked to; every other session scene
 * returns to the lobby only when the other machine is lost. */
LobbyArrival lobby_arrival(int previous_scene, int css_exit);

/* What the status line says, sorted into what is spoken. */
struct LobbyStatus {
    enum class Kind {
        searching,
        /* This network hides other players from each other. */
        hidden,
        found,
        waiting_for_host,
        connecting,
        starting,
        failed,
    };
    Kind kind = Kind::searching;
    /* failed: the base port's reason as written, without "Failed: " and the
     * retry hint. */
    std::string reason;
    /* failed: Start tries again. */
    bool retry = false;

    bool operator==(const LobbyStatus&) const = default;
};

LobbyStatus lobby_status(const A11yLobbyState& state);

class LobbySpeech {
public:
    /* speech must outlive this. */
    explicit LobbySpeech(Speech& speech) : m_speech(speech) {}

    /* The lobby scene was entered: the next snapshot is spoken as the
     * opening, after the words for the arrival. */
    void entered(LobbyArrival arrival) {
        m_arrival = arrival;
        m_seen = false;
    }

    /* The lobby's state this frame. Speaks what changed since the last one:
     * the opening interrupting, everything after it queued; nothing when
     * nothing did, and nothing outside LAN play's list of machines. */
    void frame(const A11yLobbyState& state);

private:
    struct Row {
        std::string name;
        bool other_version;

        bool operator==(const Row&) const = default;
    };

    void opening(const std::vector<Row>& rows, const LobbyStatus& status);
    void changes(const std::vector<Row>& rows, const LobbyStatus& status);

    Speech& m_speech;
    LobbyArrival m_arrival = LobbyArrival::from_menu;
    bool m_seen = false;
    std::vector<Row> m_rows;
    LobbyStatus m_status;
};

}  // namespace a11y
