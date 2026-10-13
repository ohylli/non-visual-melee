/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cue_settings.hpp"
#include <algorithm>
#include <cctype>

namespace a11y {
namespace {

constexpr const char* kGroup = "cues";

/* "Your position cue" as it reads inside a sentence. */
std::string in_sentence(std::string title) {
    if (!title.empty()) {
        title[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(title[0])));
    }
    return title;
}

bool touches(const std::vector<const Setting*>& changed, const Setting& setting) {
    return std::find(changed.begin(), changed.end(), &setting) != changed.end();
}

}  // namespace

CueLevel CueValues::level() const {
    return CueLevel{on.on(), volume.value() / 100.0f, boost.value()};
}

CueSettings::CueSettings(Settings& settings)
    : m_settings(settings),
      m_on(settings.group(kGroup, "All cues")
              .add_switch(SwitchSpec{"on", "Cues", "Turns every cue on or off.", true})),
      m_volume(settings.group(kGroup, "All cues")
              .add_number(NumberSpec{"volume", "Cue volume",
                  "The loudness of all cues together, under the master volume; the game's "
                  "sound effect and music volumes leave cues alone.",
                  "%", 100.0f, 0.0f, 100.0f, 5.0f})) {}

CueGroup CueSettings::add_cue(
    const std::string& id, const std::string& title, float default_boost) {
    SettingsGroup group = m_settings.group(id, title);
    std::string name = in_sentence(title);
    Switch on =
        group.add_switch(SwitchSpec{"on", title, "Turns the " + name + " on or off.", true});
    Number volume = group.add_number(NumberSpec{"volume", title + " volume",
        "The loudness of the " + name + ", under the cue volume.", "%", 100.0f, 0.0f, 100.0f,
        5.0f});
    std::string lifts = "Lifts the " + name +
                        " under its volume, for a sound that reads quieter "
                        "than the others at the same volume.";
    Number boost = group.add_number(NumberSpec{
        "boost", title + " boost", lifts, "times", default_boost, 1.0f, kMostBoost, 0.1f});
    CueValues values{static_cast<int>(m_cues.size()), on, volume, boost};
    m_cues.push_back(values);
    return CueGroup{group, values};
}

CueLevel CueSettings::mix_level() const {
    return CueLevel{m_on.on(), m_volume.value() / 100.0f, 1.0f};
}

void CueSettings::apply(CueEngine& engine) const {
    engine.set_mix_level(mix_level());
    for (const CueValues& cue : m_cues) {
        engine.set_cue_level(cue.cue, cue.level());
    }
}

void CueSettings::apply(CueEngine& engine, const std::vector<const Setting*>& changed) const {
    if (touches(changed, m_on.setting()) || touches(changed, m_volume.setting())) {
        engine.set_mix_level(mix_level());
    }
    for (const CueValues& cue : m_cues) {
        if (touches(changed, cue.on.setting()) || touches(changed, cue.volume.setting()) ||
            touches(changed, cue.boost.setting()))
        {
            engine.set_cue_level(cue.cue, cue.level());
        }
    }
}

}  // namespace a11y
