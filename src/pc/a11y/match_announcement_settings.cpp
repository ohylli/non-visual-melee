/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "match_announcement_settings.hpp"
#include <cmath>
#include <string>

namespace a11y {
namespace {

/* The group, its shown rows going into `menu`, else on the top level. */
ValueGroup group(ValueModel& model, const NamedMenu* menu = nullptr) {
    return model.group("match_announcements", "Match announcements").in(menu);
}

/* In DamageMode's order. */
const ChoiceOptions kDamageModes{{"Off", ""}, {"Every change", ""}, {"Thresholds", ""}};

Choice add_damage(ValueGroup group, const char* name, const char* label, const char* how) {
    return group.add_choice(ChoiceSpec{
        .name = name,
        .label = label,
        .description = std::string("How ") + how +
                       ": Every change says each new figure once it stops changing; Thresholds, "
                       "only once it passes a multiple of the damage interval.",
        .default_option = "Every change",
        .options = kDamageModes,
        .shown = true,
    });
}

DamageMode damage_mode(const Choice& choice) {
    const int index = choice_index(choice.named(), kDamageModes);
    return index < 0 ? DamageMode::every_change : static_cast<DamageMode>(index);
}

}  // namespace

MatchAnnouncementSettings::MatchAnnouncementSettings(ValueModel& model)
    : m_on(group(model).add_switch(SwitchSpec{"on", "Match announcements",
          "Says what the HUD shows during a match: your damage and stocks in your voice, everyone "
          "else's in the opponents' voice, and the time left on the screen reader.",
          true, true})),
      m_menu(group(model).add_menu(MenuSpec{"settings", "Match announcement settings",
          "The voices, and what is announced: damage, stocks, teammates and the time left."})),
      m_voice_menu(group(model, &m_menu)
              .add_menu(MenuSpec{"voices", "Voice settings",
                  "Your voice and the opponents' voice: each one's voice, rate, volume and a "
                  "preview."})),
      m_your_damage(
          add_damage(group(model, &m_menu), "your_damage", "Your damage", "your damage is said")),
      m_opponents_damage(add_damage(group(model, &m_menu), "opponents_damage", "Opponents' damage",
          "everyone else's damage is said, teammates included")),
      m_interval(group(model, &m_menu)
              .add_number(NumberSpec{"damage_interval", "Damage interval",
                  "The spacing of the damage thresholds: with Thresholds, damage is said once it "
                  "passes "
                  "a multiple of this.",
                  "%", 20.0f, 10.0f, 100.0f, 10.0f, true})),
      m_your_stocks(group(model, &m_menu)
              .add_switch(SwitchSpec{"your_stocks", "Your stocks",
                  "Says your stocks left each time they change, in a match that counts stocks.",
                  true, true})),
      m_opponents_stocks(group(model, &m_menu)
              .add_switch(SwitchSpec{"opponents_stocks", "Opponents' stocks",
                  "Says everyone else's stocks left each time they change, teammates included.",
                  true, true})),
      m_teammates(group(model, &m_menu)
              .add_switch(SwitchSpec{"teammates", "Teammates",
                  "Says your teammates' damage and stocks in a team match, as the opponents' rows "
                  "set. "
                  "Off, teammates are left out.",
                  true, true})),
      m_time_left(group(model, &m_menu)
              .add_switch(SwitchSpec{"time_left", "Time left",
                  "Says the time left at each whole minute and at 30 seconds, in a match whose "
                  "timer "
                  "counts down.",
                  true, true})),
      m_settle(group(model).add_number(NumberSpec{"damage_settle", "Damage settle time",
          "How long a player's damage must hold still before it is said, so a combo says only "
          "where it ends.",
          "s", 0.6f, 0.0f, 3.0f, 0.1f})) {}

MatchAnnouncementValues MatchAnnouncementSettings::values() const {
    MatchAnnouncementValues values;
    values.on = m_on.on();
    values.your_damage = damage_mode(m_your_damage);
    values.opponents_damage = damage_mode(m_opponents_damage);
    values.damage_interval = static_cast<int>(std::lround(m_interval.value()));
    values.your_stocks = m_your_stocks.on();
    values.opponents_stocks = m_opponents_stocks.on();
    values.teammates = m_teammates.on();
    values.time_left = m_time_left.on();
    values.settle =
        std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(m_settle.value()));
    return values;
}

}  // namespace a11y
