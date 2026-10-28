/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The switches (.scratch/accessibility-settings/spec.md, "Switches"):
 * Accessibility, Speech, Steering and Cues, settings like any other, live
 * while the game runs. Accessibility is above the other three and Cues above
 * each cue's own switch: a higher switch off silences the ones below it
 * without changing their saved values. An override from the environment
 * (MELEE_A11Y, MELEE_A11Y_STEER, MELEE_A11Y_CUES) holds its switch for the
 * run; one on a lower switch also holds it against Accessibility, so an agent
 * run steers and plays cues with speech off.
 *
 * This works out what the switches leave on and sets speech by it, with the
 * last announcement before silence and the first after it. The hooks hand
 * the rest on: steering to the screens that steer, the cues' switch to the
 * cue system, and speech's return to the readers, which announce what has
 * focus. Game thread only. */
#pragma once
#include "value_model.hpp"
#include <functional>
#include <string>

namespace a11y {

class AccessibilitySettings;
class Speech;
struct Config;

/* What the switches leave on, the hierarchy and the overrides applied. */
struct SwitchState {
    bool accessibility = true;
    bool speech = true;
    bool steering = true;
    /* Every cue may sound: Cues under Accessibility. Each cue's own switch
     * is below it, in the cue engine's volume chain. */
    bool cues = true;

    bool operator==(const SwitchState&) const = default;
};

class Switches {
public:
    /* Registers accessibility.on, speech.on and steering.on, all shown,
     * each in a group of its own. Call before any other feature registers,
     * so they are the first rows and the file's first groups. after_speech
     * registers what belongs under Speech, the voices' rows, between it and
     * Steering. */
    Switches(ValueModel& model, Speech& speech, bool log,
        const std::function<void()>& after_speech = {});

    /* The environment's overrides, once every value is registered and
     * before the settings are loaded: cues.on is the cue settings'. */
    static void override_from(const Config& config, AccessibilitySettings& settings);

    /* Once, after the settings are loaded: the state they make, set
     * silently, and logged. */
    const SwitchState& start();
    /* After any change to the settings: works the state out again. Speech
     * turned off says "Accessibility off" or "Speech off" first, its last
     * announcement; turned on again, "Accessibility on" or "Speech on" first.
     * Returns the new state; state() before the call holds the old one.
     * Before start(), nothing: the settings' load reports its changes too. */
    const SwitchState& update();
    const SwitchState& state() const { return m_state; }

    /* Whether Accessibility lets the cues sound, for the cue settings'
     * mix level, which applies Cues itself: Accessibility on, or Cues held
     * by its override. */
    bool cues_allowed() const;

    static constexpr const char* kAccessibilityKey = "accessibility.on";
    static constexpr const char* kSpeechKey = "speech.on";
    static constexpr const char* kSteeringKey = "steering.on";
    static constexpr const char* kCuesKey = "cues.on";

private:
    SwitchState read() const;
    void log_state(const char* when) const;

    ValueModel& m_model;
    Speech& m_speech;
    bool m_log;
    Switch m_accessibility;
    Switch m_speech_on;
    Switch m_steering;
    SwitchState m_state;
    bool m_started = false;
};

}  // namespace a11y
