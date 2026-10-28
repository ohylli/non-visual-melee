/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "match_announcer.hpp"
#include "game_text.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <algorithm>

namespace a11y {

void MatchAnnouncer::scene_entered() {
    m_stage = Stage::waiting;
    m_labelled = false;
    m_players = {};
}

void MatchAnnouncer::tick(
    const A11yMatchHud& hud, const MatchAnnouncementValues& values, Clock::time_point now) {
    if (m_stage == Stage::over) {
        return;
    }
    if (m_stage == Stage::waiting) {
        /* GO ends as the HUD comes on; until then nobody can be hit. */
        if (hud.match && hud.hud_enabled && hud.phase == 0) {
            start(hud, now);
        }
        return;
    }
    if (!hud.match) {
        /* Left from the pause with L+R+A+Start, which never reaches
         * GAME!, or the scene gone. */
        finish("match gone", values);
        return;
    }
    if (hud.phase != 0) {
        finish("match over", values);
        return;
    }
    for (int slot = 0; slot < A11Y_MATCH_SLOTS; slot++) {
        const A11yMatchHudPlayer& seen = hud.players[slot];
        Player& player = m_players[slot];
        if (!seen.present || !seen.panel) {
            /* Never announced without a panel; a panel that comes back
             * starts afresh from what it shows. */
            player.tracked = false;
            continue;
        }
        if (!player.tracked) {
            follow(hud, slot, now);
            continue;
        }
        stocks(hud, slot, values);
        if (seen.damage != player.shown) {
            player.shown = seen.damage;
            player.since = now;
        }
        player.out = seen.out;
        if (seen.out) {
            /* The KO zeroes the damage, on its tick or at the end of a star
             * KO's flight: the respawn's 0% is never said. */
            player.said = 0;
            continue;
        }
        if (!values.on || damage_mode(player, values) == DamageMode::off || seen.stamina) {
            /* Nothing owed: turned on again, only what changes after is
             * said. Stamina's HP is a later issue's. */
            player.said = player.shown;
            continue;
        }
        if (!hud.hud_enabled) {
            /* The HUD hidden mid-match (the pause camera, a scripted
             * scene): held until it shows again. */
            continue;
        }
        if (player.shown == player.said || now - player.since < values.settle) {
            continue;
        }
        if (owed(player, values)) {
            player.said = player.shown;
            m_speech.announce_voice(
                player.yours ? Voice::yours : Voice::opponents, slot, line(player, player.shown));
        } else if (player.shown < player.said) {
            /* Thresholds: a fall is silent but lowers the figure the next
             * crossing counts from, so 85, healed to 55, hit to 65 says
             * "65". A rise short of a threshold leaves it. */
            player.said = player.shown;
        }
    }
}

void MatchAnnouncer::start(const A11yMatchHud& hud, Clock::time_point now) {
    m_stage = Stage::running;
    int others = 0;
    for (int slot = 0; slot < A11Y_MATCH_SLOTS; slot++) {
        const A11yMatchHudPlayer& seen = hud.players[slot];
        if (slot != hud.you && seen.present && seen.panel) {
            others++;
        }
    }
    m_labelled = hud.you < 0 || others >= 2;
    for (int slot = 0; slot < A11Y_MATCH_SLOTS; slot++) {
        m_players[slot] = Player{};
        const A11yMatchHudPlayer& seen = hud.players[slot];
        if (seen.present && seen.panel) {
            follow(hud, slot, now);
        }
    }
}

void MatchAnnouncer::follow(const A11yMatchHud& hud, int slot, Clock::time_point now) {
    const A11yMatchHudPlayer& seen = hud.players[slot];
    Player& player = m_players[slot];
    player.tracked = true;
    player.yours = slot == hud.you;
    player.teammate = !player.yours && teammate(hud, slot);
    player.stocks = seen.stocks;
    player.shown = seen.damage;
    player.out = seen.out;
    player.said = seen.out ? 0 : seen.damage;
    player.since = now;
    player.label = player.yours || !m_labelled ? std::string() : label(hud, slot);
}

std::string MatchAnnouncer::label(const A11yMatchHud& hud, int slot) const {
    const A11yMatchHudPlayer& seen = hud.players[slot];
    std::string name;
    if (seen.human && !hud.online && seen.tag[0] != '\0') {
        /* Online each copy reads the tag from its own memory card. */
        name = spoken_text(decode_shift_jis(seen.tag).text);
    }
    if (name.empty()) {
        /* Over the fighter the game draws "2P", or "CP" with no number for
         * every computer player, so a CPU goes by its slot. */
        name = seen.human ? "P" + std::to_string(seen.port + 1) : "CPU " + std::to_string(slot + 1);
    }
    return teammate(hud, slot) ? "teammate " + name : name;
}

bool MatchAnnouncer::teammate(const A11yMatchHud& hud, int slot) {
    /* The slots' teams (Player_GetTeam), as the cues' nearest opponent
     * skips teammates. */
    return hud.teams && hud.you >= 0 && slot != hud.you && hud.players[hud.you].present &&
           hud.players[slot].team == hud.players[hud.you].team;
}

DamageMode MatchAnnouncer::damage_mode(
    const Player& player, const MatchAnnouncementValues& values) const {
    if (player.teammate && !values.teammates) {
        return DamageMode::off;
    }
    return player.yours ? values.your_damage : values.opponents_damage;
}

bool MatchAnnouncer::stocks_on(const Player& player, const MatchAnnouncementValues& values) const {
    if (player.teammate && !values.teammates) {
        return false;
    }
    return player.yours ? values.your_stocks : values.opponents_stocks;
}

void MatchAnnouncer::stocks(
    const A11yMatchHud& hud, int slot, const MatchAnnouncementValues& values) {
    const int count = hud.players[slot].stocks;
    Player& player = m_players[slot];
    if (count == player.stocks) {
        return;
    }
    /* Off, or no stocks on the HUD (a timed match): taken as said. */
    const bool on = hud.stocks && values.on && stocks_on(player, values);
    if (on && !hud.hud_enabled) {
        /* The HUD hidden mid-match: held until it shows again, as damage
         * is. */
        return;
    }
    player.stocks = count;
    if (on && count > 0) {
        /* On the tick it changes, a KO's or a steal's: a stock line about
         * the player takes the place of a damage line still waiting. At 0
         * the game's announcer calls the elimination. */
        m_speech.announce_voice(
            player.yours ? Voice::yours : Voice::opponents, slot, stock_line(player, count));
    }
}

bool MatchAnnouncer::owed(const Player& player, const MatchAnnouncementValues& values) const {
    switch (damage_mode(player, values)) {
    case DamageMode::every_change:
        return player.shown != player.said;
    case DamageMode::thresholds: {
        /* Counted in whole multiples, so a change of interval partway
         * through counts from the last figure said at the new spacing. */
        const int interval = std::max(values.damage_interval, 1);
        return player.shown / interval > player.said / interval;
    }
    case DamageMode::off:
        break;
    }
    return false;
}

void MatchAnnouncer::finish(const char* why, const MatchAnnouncementValues& values) {
    m_stage = Stage::over;
    for (const Player& player : m_players) {
        if (player.tracked && !player.out && values.on && owed(player, values) && log_enabled()) {
            pc_log_line("[a11y] match announcement %s dropped (%s): \"%s\"",
                voice_name(player.yours ? Voice::yours : Voice::opponents), why,
                line(player, player.shown).c_str());
        }
    }
}

std::string MatchAnnouncer::line(const Player& player, int figure) const {
    std::string figure_text = std::to_string(figure);
    return player.label.empty() ? figure_text : player.label + " " + figure_text;
}

std::string MatchAnnouncer::stock_line(const Player& player, int stocks) const {
    std::string count = stocks == 1 ? "last stock" : std::to_string(stocks) + " stocks";
    return player.label.empty() ? count : player.label + ", " + count;
}

}  // namespace a11y
