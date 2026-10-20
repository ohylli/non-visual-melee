/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "value_file.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <system_error>
#include <thread>
#include <utility>

namespace a11y {
namespace {

/* A save's rename, tried again while another program has the file open. */
constexpr int kRenameAttempts = 5;
constexpr std::chrono::milliseconds kRenameRetry{10};

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

ValueFile::ValueFile(
    ValueModel& model, Speech& speech, std::string name, std::string path, bool log)
    : m_model(model), m_speech(speech), m_name(std::move(name)), m_path(utf8_path(path)),
      m_log(log) {}

std::vector<const NamedValue*> ValueFile::load(Clock::time_point now) {
    m_next_check = now + kPollInterval;
    log("file " + utf8_string(m_path));
    if (!ensure_written()) {
        return {};
    }
    return read(true);
}

std::vector<const NamedValue*> ValueFile::poll(Clock::time_point now) {
    if (now < m_next_check) {
        return {};
    }
    m_next_check = now + kPollInterval;
    return check();
}

std::vector<const NamedValue*> ValueFile::set(
    const NamedValue& named, float value, Clock::time_point now) {
    m_next_check = now + kPollInterval;
    std::vector<const NamedValue*> changed = check();
    if (!m_model.set(named, value)) {
        return changed;
    }
    changed.erase(std::remove(changed.begin(), changed.end(), &named), changed.end());
    changed.push_back(&named);
    if (!m_read) {
        log("could not read " + utf8_string(m_path) + " to save " + named.key + "; " + named.key +
            " = " + format_value(named) + " until the game closes");
        return changed;
    }
    if (save(m_model.edit_text(m_text, named))) {
        log("saved " + named.key + " = " + format_value(named));
    } else {
        log("could not save " + named.key + " = " + format_value(named) + " to " +
            utf8_string(m_path));
    }
    return changed;
}

bool ValueFile::save(const std::string& text) {
    std::filesystem::path temporary = m_path;
    temporary += ".tmp";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out << text;
    out.close();
    std::error_code error;
    if (!out) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    /* On Windows the rename fails while another program holds the file open
     * without sharing its deletion, as an editor, a virus scanner or the
     * drive scripts' ini step may for a moment: tried again a few times. */
    for (int attempt = 0; attempt < kRenameAttempts; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(kRenameRetry);
        }
        std::filesystem::rename(temporary, m_path, error);
        if (!error) {
            break;
        }
    }
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    /* What the poll compares against: the next one finds the file as saved,
     * so it reads nothing and says nothing (spec open question 1). */
    m_text = text;
    m_stamped = stamp(m_stamp);
    return true;
}

std::vector<const NamedValue*> ValueFile::check() {
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

bool ValueFile::ensure_written() {
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
                       "# A change in the game's menus edits only its own line here.\n" +
                       "# Delete the file to get every value back at its default.\n";
    text += m_model.to_text(true);
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

std::vector<const NamedValue*> ValueFile::read(bool append_missing) {
    std::ifstream in(m_path, std::ios::binary);
    if (!in) {
        log("could not read " + utf8_string(m_path));
        m_read = false;
        return {};
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    m_stamped = stamp(m_stamp);
    if (!append_missing && m_read && text == m_text) {
        return {};
    }
    m_text = text;
    m_read = true;
    TextRead result = m_model.read_text(text);
    report(result);
    if (append_missing && !result.missing.empty()) {
        /* Values of a newer version: added at the end, so the player finds
         * them, and nothing they wrote is touched. */
        std::string added = m_model.to_text(result.missing);
        std::ofstream out(m_path, std::ios::binary | std::ios::app);
        if (!text.empty() && text.back() != '\n') {
            out << "\n";
        }
        out << added;
        out.close();
        std::string keys;
        for (const NamedValue* named : result.missing) {
            keys += (keys.empty() ? "" : ", ") + named->key;
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
    for (const NamedValue* named : result.changed) {
        log(named->key + " = " + format_value(*named));
    }
    return result.changed;
}

bool ValueFile::stamp(Stamp& out) const {
    std::error_code error;
    out.time = std::filesystem::last_write_time(m_path, error);
    if (error) {
        return false;
    }
    out.size = std::filesystem::file_size(m_path, error);
    return !error;
}

void ValueFile::report(const TextRead& read) {
    for (const std::string& note : read.notes) {
        log(note);
    }
    for (const std::string& key : read.unknown) {
        if (std::find(m_unknown_logged.begin(), m_unknown_logged.end(), key) ==
            m_unknown_logged.end())
        {
            m_unknown_logged.push_back(key);
            log(key + " is not a known key; kept in the file");
        }
    }
    m_problem.clear();
    if (!read.bad_lines.empty()) {
        m_problem = m_name + ", " + line_list(read.bad_lines) + " not understood.";
        m_speech.announce(m_problem, Mode::queue);
        m_problems_spoken++;
    }
}

void ValueFile::log(const std::string& line) const {
    if (m_log) {
        pc_log_line("[a11y] %s: %s", lowered(m_name).c_str(), line.c_str());
    }
}

}  // namespace a11y
