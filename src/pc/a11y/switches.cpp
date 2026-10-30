/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "switches.hpp"
#include "accessibility_settings.hpp"
#include "pc/pc.h"
#include "speech.hpp"

namespace a11y {
namespace {

/* A switch under its parent: off with the parent, unless an override holds
 * it, which holds against the parent too. */
bool under(bool parent, const NamedValue& child) {
    return child.overridden ? child.value != 0.0f : parent && child.value != 0.0f;
}

const char* on_off(bool on) {
    return on ? "on" : "off";
}

/* " (MELEE_A11Y=0)" after a switch its override holds. */
std::string held_by(const NamedValue* named, const char* variable) {
    if (named == nullptr || !named->overridden) {
        return "";
    }
    return std::string(" (") + variable + "=" + (named->value != 0.0f ? "1" : "0") + ")";
}

}  // namespace

Switches::Switches(ValueModel& model, Speech& speech, bool log)
    : m_model(model), m_speech(speech), m_log(log),
      m_accessibility(model.group("accessibility", "Accessibility")
              .add_switch(SwitchSpec{"on", "Accessibility",
                  "Turns everything Non-Visual Melee adds on or off: speech, steering and the "
                  "cues.",
                  true, true})),
      m_speech_on(model.group("speech", "Speech")
              .add_switch(SwitchSpec{"on", "Speech",
                  "Turns speech on or off, through the screen reader and the voices.", true,
                  true})),
      m_steering(model.group("steering", "Steering")
              .add_switch(SwitchSpec{"on", "Steering",
                  "The D-pad steps the hand on character select and the cursor on stage "
                  "select. Off, the D-pad does what the game makes of it, quick chat "
                  "included.",
                  true, true})) {}

void Switches::override_from(const Config& config, AccessibilitySettings& settings) {
    if (config.accessibility.has_value()) {
        settings.override_value(kAccessibilityKey, *config.accessibility ? 1.0f : 0.0f,
            std::string("MELEE_A11Y=") + (*config.accessibility ? "1" : "0"));
    }
    if (config.steering.has_value()) {
        settings.override_value(kSteeringKey, *config.steering ? 1.0f : 0.0f,
            std::string("MELEE_A11Y_STEER=") + (*config.steering ? "1" : "0"));
    }
    if (config.cues.has_value()) {
        settings.override_value(kCuesKey, *config.cues ? 1.0f : 0.0f,
            std::string("MELEE_A11Y_CUES=") + (*config.cues ? "1" : "0"));
    }
}

bool Switches::cues_allowed() const {
    const NamedValue* cues = m_model.find(kCuesKey);
    return m_accessibility.on() || (cues != nullptr && cues->overridden);
}

SwitchState Switches::read() const {
    SwitchState state;
    state.accessibility = m_accessibility.on();
    state.speech = under(state.accessibility, m_speech_on.named());
    state.steering = under(state.accessibility, m_steering.named());
    const NamedValue* cues = m_model.find(kCuesKey);
    state.cues = cues != nullptr ? under(state.accessibility, *cues) : state.accessibility;
    return state;
}

const SwitchState& Switches::start() {
    m_started = true;
    m_state = read();
    m_speech.set_on(m_state.speech);
    log_state("start");
    return m_state;
}

const SwitchState& Switches::update() {
    if (!m_started) {
        return m_state;
    }
    const SwitchState was = m_state;
    const SwitchState now = read();
    if (now == was) {
        return m_state;
    }
    /* Named by the switch that did it: Accessibility when it changed. */
    const bool by_accessibility = now.accessibility != was.accessibility;
    if (was.speech && !now.speech) {
        /* Said while speech is still on, then nothing more is sent, so
         * nothing cuts it off. */
        m_speech.announce(by_accessibility ? "Accessibility off" : "Speech off", Mode::interrupt);
        m_speech.set_on(false);
    } else if (!was.speech && now.speech) {
        m_speech.set_on(true);
        m_speech.announce(by_accessibility ? "Accessibility on" : "Speech on", Mode::interrupt);
    }
    m_state = now;
    log_state("now");
    return m_state;
}

void Switches::log_state(const char* when) const {
    if (!m_log) {
        return;
    }
    const std::string accessibility = held_by(&m_accessibility.named(), "MELEE_A11Y");
    const std::string steering = held_by(&m_steering.named(), "MELEE_A11Y_STEER");
    const std::string cues = held_by(m_model.find(kCuesKey), "MELEE_A11Y_CUES");
    pc_log_line("[a11y] switches %s: accessibility %s%s, speech %s, steering %s%s, cues %s%s", when,
        on_off(m_state.accessibility), accessibility.c_str(), on_off(m_state.speech),
        on_off(m_state.steering), steering.c_str(), on_off(m_state.cues), cues.c_str());
}

}  // namespace a11y
