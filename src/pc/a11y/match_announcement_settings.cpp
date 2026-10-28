/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "match_announcement_settings.hpp"

namespace a11y {
namespace {

ValueGroup group(ValueModel& model) {
    return model.group("match_announcements", "Match announcements");
}

}  // namespace

MatchAnnouncementSettings::MatchAnnouncementSettings(ValueModel& model)
    : m_on(group(model).add_switch(SwitchSpec{"on", "Match announcements",
          "Says what the HUD shows during a match: your damage in your voice, everyone else's in "
          "the opponents' voice.",
          true, true})),
      m_settle(group(model).add_number(NumberSpec{"damage_settle", "Damage settle time",
          "How long a player's damage must hold still before it is said, so a combo says only "
          "where it ends.",
          "s", 0.6f, 0.0f, 3.0f, 0.1f})) {}

MatchAnnouncementValues MatchAnnouncementSettings::values() const {
    MatchAnnouncementValues values;
    values.on = m_on.on();
    values.settle =
        std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(m_settle.value()));
    return values;
}

}  // namespace a11y
