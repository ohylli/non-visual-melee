/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The settings file: the settings model's first front end (primer
 * docs/a11y/cues.md, "Cue settings"). Plain text the player edits while the
 * game runs, re-read about twice a second when it changes. The port menu is
 * meant to become a second front end over the same model. Game thread
 * only. */
#pragma once
#include "settings.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace a11y {

class Speech;

using Clock = std::chrono::steady_clock;

class SettingsFile {
public:
    /* name heads the file and starts spoken errors: "Cue settings". path is
     * UTF-8. */
    SettingsFile(Settings& settings, Speech& speech, std::string name, std::string path, bool log);

    /* Once, after every value is registered: writes the file with every
     * value at its default when it is missing, appends the values it lacks
     * (a newer version's), then applies it. Returns the values it changed. */
    std::vector<const Setting*> load(Clock::time_point now);
    /* As often as it suits the caller: looks at the file about twice a
     * second, reads it only when it changed and applies only the values
     * that changed, which it returns. A file deleted meanwhile is written
     * afresh with the defaults. */
    std::vector<const Setting*> poll(Clock::time_point now);

    /* The last read's error announcement ("Cue settings, line 12 not
     * understood."), empty when it read cleanly: already spoken once, kept
     * to repeat where it will be heard, since speech at startup or at a
     * scene change is easily cut off. */
    const std::string& problem() const { return m_problem; }
    /* How many times a read's error has been spoken so far, so a caller can
     * tell whether it was said since a moment it marked. */
    int problems_spoken() const { return m_problems_spoken; }

    static constexpr std::chrono::milliseconds kPollInterval{500};

private:
    struct Stamp {
        std::filesystem::file_time_type time{};
        std::uintmax_t size = 0;
        bool operator==(const Stamp&) const = default;
    };

    /* Writes the defaults when the file is missing; false when it is still
     * missing. */
    bool ensure_written();
    std::vector<const Setting*> read(bool append_missing);
    bool stamp(Stamp& out) const;
    void report(const SettingsRead& read);
    void log(const std::string& line) const;

    Settings& m_settings;
    Speech& m_speech;
    std::string m_name;
    std::filesystem::path m_path;
    bool m_log;
    Clock::time_point m_next_check{};
    Stamp m_stamp;
    bool m_stamped = false;
    std::string m_text; /* the text last read */
    std::vector<std::string> m_unknown_logged;
    std::string m_problem;
    int m_problems_spoken = 0;
};

}  // namespace a11y
