/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The model's file (primer docs/a11y/cues.md, "Accessibility settings"):
 * plain text the player edits while the game runs, re-read about twice a
 * second when it changes, and saved one line at a time when a menu changes a
 * value. Game thread only. */
#pragma once
#include "value_model.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace a11y {

class Speech;

using Clock = std::chrono::steady_clock;

class ValueFile {
public:
    /* name heads the file and starts spoken errors: "Accessibility
     * settings". path is UTF-8. */
    ValueFile(ValueModel& model, Speech& speech, std::string name, std::string path, bool log);

    /* Once, after every value is registered: writes the file with every
     * value at its default when it is missing, appends the values it lacks
     * (a newer version's), then applies it. Returns the values it changed. */
    std::vector<const NamedValue*> load(Clock::time_point now);
    /* As often as it suits the caller: looks at the file about twice a
     * second, reads it only when it changed and applies only the values
     * that changed, which it returns. A file deleted meanwhile is written
     * afresh with the defaults. */
    std::vector<const NamedValue*> poll(Clock::time_point now);
    /* A front end's change, held to the value's range and saved by editing
     * its line alone (ValueModel::edit_text), through a temporary file and
     * a rename, so a crash halfway leaves the old file whole. The file is
     * read first, whatever the interval, so a hand edit not yet polled is
     * applied and kept rather than lost under the save. Returns the values
     * that changed, that hand edit's included; the poll after it finds the
     * file as saved and reads nothing. */
    std::vector<const NamedValue*> set(const NamedValue& named, float value, Clock::time_point now);

    /* The last read's error announcement ("Accessibility settings, line 12
     * not understood."), empty when it read cleanly: already spoken once, kept
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
    /* Reads the file if it changed, or wrote it afresh when it is missing;
     * the poll and the set share it. */
    std::vector<const NamedValue*> check();
    std::vector<const NamedValue*> read(bool append_missing);
    /* The text in place of the file's, by a temporary file and a rename. */
    bool save(const std::string& text);
    bool stamp(Stamp& out) const;
    void report(const TextRead& read);
    void log(const std::string& line) const;

    ValueModel& m_model;
    Speech& m_speech;
    std::string m_name;
    std::filesystem::path m_path;
    bool m_log;
    Clock::time_point m_next_check{};
    Stamp m_stamp;
    bool m_stamped = false;
    std::string m_text;  /* the text last read or saved */
    bool m_read = false; /* m_text is the file's */
    std::vector<std::string> m_unknown_logged;
    std::string m_problem;
    int m_problems_spoken = 0;
};

}  // namespace a11y
