/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The match announcements' rows in the accessibility settings
 * (.scratch/match-announcements/spec.md, "The settings rows";
 * .scratch/accessibility-submenus/spec.md, "The layout"): on the top level
 * the Match announcements switch, over every match announcement and itself
 * under Speech, then the row opening Match announcement settings, the
 * submenu that holds the rest: first the row opening Voice settings, whose
 * rows the voices register (VoiceSettings); Your damage and Opponents' damage,
 * each a choice of Off, Every change or Thresholds; the Damage interval both
 * share; Your stocks, Opponents' stocks, Teammates and Time left, switches;
 * and the settle time, a tuning value.
 * The announcer reads them as they stand each tick, so a change takes effect
 * at once. Game thread only. */
#pragma once
#include "match_announcer.hpp"
#include "value_model.hpp"

namespace a11y {

class MatchAnnouncementSettings {
public:
    /* Registers the rows in the match_announcements group. Call after the
     * cues' rows, so the switch follows Cue settings on the top level. */
    explicit MatchAnnouncementSettings(ValueModel& model);

    /* The values as they stand now. */
    MatchAnnouncementValues values() const;

    /* Match announcement settings, and Voice settings inside it, for the
     * voices' rows. */
    const NamedMenu& menu() const { return m_menu; }
    const NamedMenu& voice_menu() const { return m_voice_menu; }

    static constexpr const char* kOnKey = "match_announcements.on";
    static constexpr const char* kYourDamageKey = "match_announcements.your_damage";
    static constexpr const char* kOpponentsDamageKey = "match_announcements.opponents_damage";
    static constexpr const char* kDamageIntervalKey = "match_announcements.damage_interval";
    static constexpr const char* kYourStocksKey = "match_announcements.your_stocks";
    static constexpr const char* kOpponentsStocksKey = "match_announcements.opponents_stocks";
    static constexpr const char* kTeammatesKey = "match_announcements.teammates";
    static constexpr const char* kTimeLeftKey = "match_announcements.time_left";

private:
    Switch m_on;
    const NamedMenu& m_menu;
    const NamedMenu& m_voice_menu;
    Choice m_your_damage;
    Choice m_opponents_damage;
    Number m_interval;
    Switch m_your_stocks;
    Switch m_opponents_stocks;
    Switch m_teammates;
    Switch m_time_left;
    Number m_settle;
};

}  // namespace a11y
