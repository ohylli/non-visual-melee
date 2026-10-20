/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "accessibility_settings.hpp"
#include "pc/pc.h"
#include <algorithm>
#include <exception>

namespace a11y {

AccessibilitySettings::AccessibilitySettings(Speech& speech, const std::string& path, bool log)
    : m_log(log) {
    if (path.empty()) {
        return;
    }
    /* The one place a path from outside meets std::filesystem, which throws
     * on text that is not UTF-8 (getenv gives the ANSI code page on
     * Windows); nothing may escape into the game's C. */
    try {
        m_file = std::make_unique<ValueFile>(m_model, speech, kName, path, log);
    } catch (const std::exception& error) {
        m_file.reset();
        if (log) {
            pc_log_line("[a11y] accessibility settings: cannot use %s (%s); using the defaults",
                path.c_str(), error.what());
        }
    }
}

bool AccessibilitySettings::override_value(
    std::string_view key, float value, const std::string& why) {
    const NamedValue* named = m_model.find(key);
    if (named == nullptr) {
        return false;
    }
    m_model.override_value(*named, value);
    if (m_log) {
        pc_log_line("[a11y] accessibility settings: %s = %s for this run (%s), not saved",
            named->key.c_str(), format_value(*named).c_str(), why.c_str());
    }
    return true;
}

void AccessibilitySettings::load(Clock::time_point now) {
    if (m_file != nullptr) {
        changed(m_file->load(now));
    }
}

void AccessibilitySettings::poll(Clock::time_point now) {
    if (m_file != nullptr) {
        changed(m_file->poll(now));
    }
}

bool AccessibilitySettings::set(const NamedValue& named, float value, Clock::time_point now) {
    if (m_file == nullptr) {
        if (!m_model.set(named, value)) {
            return false;
        }
        if (m_log) {
            pc_log_line("[a11y] accessibility settings: %s = %s, not saved: no file",
                named.key.c_str(), format_value(named).c_str());
        }
        changed({&named});
        return true;
    }
    std::vector<const NamedValue*> values = m_file->set(named, value, now);
    changed(values);
    return std::find(values.begin(), values.end(), &named) != values.end();
}

const std::string& AccessibilitySettings::problem() const {
    static const std::string none;
    return m_file != nullptr ? m_file->problem() : none;
}

int AccessibilitySettings::problems_spoken() const {
    return m_file != nullptr ? m_file->problems_spoken() : 0;
}

void AccessibilitySettings::changed(const std::vector<const NamedValue*>& values) {
    if (!values.empty() && m_listener) {
        m_listener(values);
    }
}

}  // namespace a11y
