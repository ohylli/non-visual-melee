/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The match announcements' rows in the accessibility settings
 * (.scratch/match-announcements/spec.md, "The settings rows"), right after
 * the voices' rows: the Match announcements switch, over every match
 * announcement and itself under Speech; Your damage and Opponents' damage,
 * each a choice of Off, Every change or Thresholds; the Damage interval both
 * share; and the settle time, a tuning value.
 * The announcer reads them as they stand each tick, so a change takes effect
 * at once. Game thread only. */
#pragma once
#include "match_announcer.hpp"
#include "value_model.hpp"

namespace a11y {

class MatchAnnouncementSettings {
public:
    /* Registers the rows in the match_announcements group. Call after the
     * voices' rows (Switches' after_speech), so they follow them. */
    explicit MatchAnnouncementSettings(ValueModel& model);

    /* The values as they stand now. */
    MatchAnnouncementValues values() const;

    static constexpr const char* kOnKey = "match_announcements.on";
    static constexpr const char* kYourDamageKey = "match_announcements.your_damage";
    static constexpr const char* kOpponentsDamageKey = "match_announcements.opponents_damage";
    static constexpr const char* kDamageIntervalKey = "match_announcements.damage_interval";

private:
    Switch m_on;
    Choice m_your_damage;
    Choice m_opponents_damage;
    Number m_interval;
    Number m_settle;
};

}  // namespace a11y
