/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cues' part of the settings model (primer docs/a11y/cues.md, "Cue
 * settings"): the values every cue shares, those each cue has (its switch,
 * volume and boost), and how they reach the cue engine's volume chain. A cue
 * registers its own group through add_cue, adds its mapping values to that
 * group, and reads them all through typed handles. Game thread only. */
#pragma once
#include "cue_engine.hpp"
#include "settings.hpp"
#include <string>
#include <vector>

namespace a11y {

/* The values every cue has, which make its level in the volume chain. */
struct CueValues {
    int cue; /* the cue's number in the engine, for its VoiceSpec */
    Switch on;
    Number volume; /* 0 to 100% */
    Number boost;  /* a factor, 1 to CueEngine::kMostBoost */

    CueLevel level() const;
};

/* A registered cue: its own group, for its mapping values, and its shared
 * values. */
struct CueGroup {
    SettingsGroup group;
    CueValues values;
};

class CueSettings {
public:
    /* Registers the values for all cues: the switch and the cue volume. */
    explicit CueSettings(Settings& settings);

    /* Registers a cue's group, titled as the file and the port menu head
     * it ("Your position cue"), with its switch, volume and the boost it
     * starts with, and gives it the next cue number. */
    CueGroup add_cue(const std::string& id, const std::string& title, float default_boost);

    /* Every level into the engine: once the settings are first read. */
    void apply(CueEngine& engine) const;
    /* Only the levels that these changed values make up. */
    void apply(CueEngine& engine, const std::vector<const Setting*>& changed) const;

    CueLevel mix_level() const;
    const std::vector<CueValues>& cues() const { return m_cues; }

private:
    Settings& m_settings;
    Switch m_on;
    Number m_volume;
    std::vector<CueValues> m_cues;
};

}  // namespace a11y
