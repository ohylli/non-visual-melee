/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cues' part of the accessibility settings (primer docs/a11y/cues.md,
 * "The cues' values"): the values every cue shares, those each cue has (its
 * switch, volume and boost), and how they reach the cue engine's volume
 * chain. A cue registers its own group through add_cue, adds its mapping
 * values to that group as tuning values, and reads them all through typed
 * handles. Game thread only. */
#pragma once
#include "cue_engine.hpp"
#include "value_model.hpp"
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
    ValueGroup group;
    CueValues values;
};

class CueSettings {
public:
    /* Registers the values for all cues, both shown: the switch and the cue
     * volume. */
    explicit CueSettings(ValueModel& model);

    /* Registers a cue's group, titled as the file heads it and the menus
     * name it ("Your position cue"), with its switch, volume and the boost it
     * starts with, and gives it the next cue number. The switch and the
     * volume are shown, followed by an action, Preview, whose group is the
     * cue's id; the boost is a tuning value. */
    CueGroup add_cue(const std::string& id, const std::string& title, float default_boost);
    /* Registers a group for values several cues share, with no cue of its
     * own: no switch, volume or cue number. */
    ValueGroup add_shared(const std::string& id, const std::string& title);

    /* Every level into the engine: once the settings are first read. */
    void apply(CueEngine& engine) const;
    /* Only the levels that these changed values make up. */
    void apply(CueEngine& engine, const std::vector<const NamedValue*>& changed) const;

    /* Whether Accessibility lets the cues sound (Switches::cues_allowed):
     * off mutes the mix whatever the Cues switch says, which keeps its
     * value. Call apply afterwards. */
    void set_allowed(bool allowed) { m_allowed = allowed; }

    CueLevel mix_level() const;
    const std::vector<CueValues>& cues() const { return m_cues; }

private:
    ValueModel& m_model;
    Switch m_on;
    Number m_volume;
    bool m_allowed = true;
    std::vector<CueValues> m_cues;
};

}  // namespace a11y
