/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The accessibility settings (primer docs/a11y/cues.md, "Accessibility
 * settings"): the fork's one value model and its file, a11y.ini. Features
 * register their values in model() as the game starts, then load() reads the
 * file; after that a front end changes a value through set(), which saves
 * its line, and poll() picks up the player's hand edits. Either way every
 * change reaches the one listener, which hands it on to the features that
 * keep copies (the cue engine's levels). Game thread only. */
#pragma once
#include "value_file.hpp"
#include "value_model.hpp"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

class Speech;

using SettingsListener = std::function<void(const std::vector<const NamedValue*>& changed)>;

class AccessibilitySettings {
public:
    /* The name that heads the file, starts its log lines and its spoken
     * errors. */
    static constexpr const char* kName = "Accessibility settings";

    /* path is UTF-8, empty for no file: every value stays at its default
     * until a front end changes it, and nothing is saved. */
    AccessibilitySettings(Speech& speech, const std::string& path, bool log);

    ValueModel& model() { return m_model; }
    const ValueModel& model() const { return m_model; }
    /* The menus' rows, in order (ValueModel::shown). */
    const std::vector<ShownRow>& shown() const { return m_model.shown(); }

    /* Told every value that changes after load, whoever changed it. */
    void listen(SettingsListener listener) { m_listener = std::move(listener); }

    /* An environment variable's override, after the value is registered and
     * before load(): the value holds for the run whatever the file says, and
     * the file keeps its own, never written over (ValueModel::override_value).
     * A front end's change lifts it. why names the variable for the log:
     * "MELEE_A11Y=0". False for an unknown key and for a choice, which has
     * none. */
    bool override_value(std::string_view key, float value, const std::string& why);
    /* Once, after every value is registered: writes or completes the file
     * and applies it (ValueFile::load). */
    void load(Clock::time_point now);
    /* As often as it suits the caller: a hand edit applied about half a
     * second after it is saved (ValueFile::poll). */
    void poll(Clock::time_point now);
    /* A front end's change, held to the value's range and saved to its
     * line. True when the value changed. */
    bool set(const NamedValue& named, float value, Clock::time_point now);
    /* A front end's choice, the option's text, saved as set saves. True
     * when the saved text changed. */
    bool choose(const NamedValue& named, std::string_view text, Clock::time_point now);
    /* Whether a change is saved: false with no file. */
    bool saves() const { return m_file != nullptr; }

    /* The file's last error, to repeat where it will be heard, and how many
     * have been spoken (ValueFile::problem); empty and 0 with no file. */
    const std::string& problem() const;
    int problems_spoken() const;

private:
    void changed(const std::vector<const NamedValue*>& values);
    /* set and choose: `apply` changes the model alone, `save` through the
     * file. */
    template <class Apply, class Save>
    bool change(const NamedValue& named, Apply apply, Save save);

    ValueModel m_model;
    std::unique_ptr<ValueFile> m_file; /* null with no file */
    SettingsListener m_listener;
    bool m_log;
};

}  // namespace a11y
