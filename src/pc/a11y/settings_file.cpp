/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings_file.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace a11y {
namespace {

std::filesystem::path utf8_path(const std::string& path) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}

std::string utf8_string(const std::filesystem::path& path) {
    std::u8string text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

/* "line 12", "lines 3 and 7", "lines 3, 7 and 12". */
std::string line_list(const std::vector<int>& lines) {
    std::string text = lines.size() == 1 ? "line " : "lines ";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            text += i + 1 == lines.size() ? " and " : ", ";
        }
        text += std::to_string(lines[i]);
    }
    return text;
}

std::string lowered(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

}  // namespace

SettingsFile::SettingsFile(
    Settings& settings, Speech& speech, std::string name, std::string path, bool log)
    : m_settings(settings), m_speech(speech), m_name(std::move(name)), m_path(utf8_path(path)),
      m_log(log) {}

std::vector<const Setting*> SettingsFile::load(Clock::time_point now) {
    m_next_check = now + kPollInterval;
    log("file " + utf8_string(m_path));
    if (!ensure_written()) {
        return {};
    }
    return read(true);
}

std::vector<const Setting*> SettingsFile::poll(Clock::time_point now) {
    if (now < m_next_check) {
        return {};
    }
    m_next_check = now + kPollInterval;
    Stamp current;
    if (!stamp(current)) {
        /* Deleted: back to the defaults, with a file to edit again. */
        if (!ensure_written()) {
            return {};
        }
    } else if (m_stamped && current == m_stamp) {
        return {};
    }
    return read(false);
}

bool SettingsFile::ensure_written() {
    std::error_code error;
    if (std::filesystem::exists(m_path, error)) {
        return true;
    }
    if (m_path.has_parent_path()) {
        std::filesystem::create_directories(m_path.parent_path(), error);
    }
    std::string text = "# " + m_name + " for Non-Visual Melee.\n" +
                       "# One value a line: key = value. Lines starting with # are comments.\n" +
                       "# The game reads this file again about half a second after it is saved.\n" +
                       "# Delete the file to get every value back at its default.\n";
    text += m_settings.to_text(true);
    std::ofstream out(m_path, std::ios::binary);
    out << text;
    out.close();
    if (!out) {
        log("could not write " + utf8_string(m_path) + "; using the defaults");
        return false;
    }
    log("written with defaults: " + utf8_string(m_path));
    return true;
}

std::vector<const Setting*> SettingsFile::read(bool append_missing) {
    std::ifstream in(m_path, std::ios::binary);
    if (!in) {
        log("could not read " + utf8_string(m_path));
        return {};
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    m_stamped = stamp(m_stamp);
    if (!append_missing && text == m_text) {
        return {};
    }
    m_text = text;
    SettingsRead result = m_settings.read_text(text);
    report(result);
    if (append_missing && !result.missing.empty()) {
        /* Values of a newer version: added at the end, so the player finds
         * them, and nothing they wrote is touched. */
        std::string added = m_settings.to_text(result.missing);
        std::ofstream out(m_path, std::ios::binary | std::ios::app);
        if (!text.empty() && text.back() != '\n') {
            out << "\n";
        }
        out << added;
        out.close();
        std::string keys;
        for (const Setting* setting : result.missing) {
            keys += (keys.empty() ? "" : ", ") + setting->key;
        }
        if (out) {
            log("added what the file lacked, at the defaults: " + keys);
            std::ifstream again(m_path, std::ios::binary);
            m_text.assign(std::istreambuf_iterator<char>(again), std::istreambuf_iterator<char>());
            m_stamped = stamp(m_stamp);
        } else {
            log("could not add what the file lacked: " + keys);
        }
    }
    for (const Setting* setting : result.changed) {
        log(setting->key + " = " + format_value(*setting));
    }
    return result.changed;
}

bool SettingsFile::stamp(Stamp& out) const {
    std::error_code error;
    out.time = std::filesystem::last_write_time(m_path, error);
    if (error) {
        return false;
    }
    out.size = std::filesystem::file_size(m_path, error);
    return !error;
}

void SettingsFile::report(const SettingsRead& read) {
    for (const std::string& note : read.notes) {
        log(note);
    }
    for (const std::string& key : read.unknown) {
        if (std::find(m_unknown_logged.begin(), m_unknown_logged.end(), key) ==
            m_unknown_logged.end())
        {
            m_unknown_logged.push_back(key);
            log(key + " is not a known setting; kept in the file");
        }
    }
    m_problem.clear();
    if (!read.bad_lines.empty()) {
        m_problem = m_name + ", " + line_list(read.bad_lines) + " not understood.";
        m_speech.announce(m_problem, Mode::queue);
        m_problems_spoken++;
    }
}

void SettingsFile::log(const std::string& line) const {
    if (m_log) {
        pc_log_line("[a11y] %s: %s", lowered(m_name).c_str(), line.c_str());
    }
}

}  // namespace a11y
