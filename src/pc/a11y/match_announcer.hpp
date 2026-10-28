/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The match announcements (.scratch/match-announcements/spec.md; primer
 * docs/a11y/gameplay-state.md, "The HUD"): what the HUD shows of each player,
 * said in the voices. Your damage in your voice; everyone else's whose HUD
 * panel shows in the opponents' voice, teammates included, each line's
 * subject the player's slot, so a newer line about a player replaces an
 * older one (voices.hpp). Fed once a tick from a snapshot (A11yMatchHud) and
 * touching no game state itself.
 *
 * A match's announcements run from GO, when the HUD comes on, to GAME! or
 * TIME!. At GO every player's damage is taken as already said, so a match
 * starting at 300% says nothing until it changes; the player labels are
 * decided then too and kept. Damage is said once it has held still for the
 * settle time, with no maximum wait, whenever it differs from the last
 * figure said ("45", "P2 45"): a combo says its end figure, a heal its
 * last. A KO resets the last figure to 0 silently, so the respawn's 0% is
 * never said. At GAME! or TIME! a figure still settling is dropped, and
 * logged as dropped. Each side's damage can instead be said only past
 * thresholds, a multiple of the interval crossed upward since the last
 * figure said (35 to 52 at 20 says "52"), or not at all. Game thread
 * only. */
#pragma once
#include "game_access.h"
#include <array>
#include <chrono>
#include <string>

namespace a11y {

class Speech;

using Clock = std::chrono::steady_clock;

/* How a side's damage is said, in the order of the rows' options. */
enum class DamageMode {
    off,
    every_change, /* the settled damage whenever it differs from the last said */
    /* The settled damage once it has crossed a multiple of the interval
     * upward since the last figure said; a fall is silent but lowers the
     * figure the next crossing counts from. */
    thresholds,
};

/* The match announcements' values as they stand, handed in each tick, so a
 * change takes effect at once, partway through a match too. */
struct MatchAnnouncementValues {
    /* The Match announcements switch: off, nothing is said, and every
     * figure is taken as said, so turning it on mid-match says only what
     * changes after. */
    bool on = true;
    DamageMode your_damage = DamageMode::every_change;
    /* Everyone else's, teammates included. */
    DamageMode opponents_damage = DamageMode::every_change;
    /* The thresholds' spacing, shared by both sides. */
    int damage_interval = 20;
    /* How long a figure must hold still before it is said. */
    Clock::duration settle = std::chrono::milliseconds(600);
};

class MatchAnnouncer {
public:
    /* speech must outlive this. */
    explicit MatchAnnouncer(Speech& speech) : m_speech(speech) {}

    /* A scene was entered: the last match is forgotten, and the next GO
     * starts a new one. */
    void scene_entered();

    /* The HUD after a tick, with the values in force and the time it was
     * read. */
    void tick(
        const A11yMatchHud& hud, const MatchAnnouncementValues& values, Clock::time_point now);

private:
    enum class Stage {
        waiting, /* for GO */
        running, /* from GO */
        over,    /* from GAME! or TIME!, or the match gone, until the next scene */
    };

    struct Player {
        /* Followed since its panel showed: a figure is owed only from
         * there. */
        bool tracked = false;
        bool yours = false;
        /* The figure the HUD showed last tick, and since when. */
        int shown = 0;
        Clock::time_point since{};
        /* The last figure said, or taken as said: the one a threshold's
         * crossing counts from, lowered by a fall. */
        int said = 0;
        /* Knocked out last tick: nothing owed until the respawn. */
        bool out = false;
        /* "P2", "teammate Bob", or empty where no label is said. */
        std::string label;
    };

    void start(const A11yMatchHud& hud, Clock::time_point now);
    /* Starts following a player whose panel shows: the figure shown taken
     * as said. */
    void follow(const A11yMatchHud& hud, int slot, Clock::time_point now);
    /* The player's label in the opponents' voice, decided at GO: empty for
     * one opponent and nobody else. */
    std::string label(const A11yMatchHud& hud, int slot) const;
    DamageMode damage_mode(const Player& player, const MatchAnnouncementValues& values) const;
    /* Whether the player's figure shown now is to be said once it settles,
     * by the mode in force. */
    bool owed(const Player& player, const MatchAnnouncementValues& values) const;
    /* The match is over: each figure still owed is dropped, logged with
     * why. */
    void finish(const char* why, const MatchAnnouncementValues& values);
    std::string line(const Player& player, int figure) const;

    Speech& m_speech;
    Stage m_stage = Stage::waiting;
    /* Lines in the opponents' voice start with the player's label: two or
     * more players besides you at GO, or no "you". */
    bool m_labelled = false;
    std::array<Player, A11Y_MATCH_SLOTS> m_players{};
};

}  // namespace a11y
