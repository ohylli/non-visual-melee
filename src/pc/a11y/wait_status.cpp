/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "wait_status.hpp"
#include <cctype>

namespace a11y {
namespace {

/* Timed by ear; the play test may move them. */
constexpr unsigned kFirstMoment = 15;
constexpr unsigned kSecondMoment = 30;
constexpr unsigned kRhythmStart = 75;
constexpr unsigned kRhythm = 30;

bool is_digit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

/* "m:ss", as the base port prints the clock. */
bool is_clock(std::string_view word) {
    size_t colon = word.find(':');
    if (colon == std::string_view::npos || colon == 0 || word.size() - colon != 3) {
        return false;
    }
    for (size_t i = 0; i < word.size(); i++) {
        if (i != colon && !is_digit(word[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace

unsigned reminder_moment(unsigned seconds) {
    if (seconds < kFirstMoment) {
        return 0;
    }
    if (seconds < kSecondMoment) {
        return kFirstMoment;
    }
    if (seconds < kRhythmStart) {
        return kSecondMoment;
    }
    return kRhythmStart + (seconds - kRhythmStart) / kRhythm * kRhythm;
}

std::string StatusLine::shown() const {
    if (clock.empty()) {
        return words;
    }
    return words.empty() ? clock : words + " " + clock;
}

int StatusLine::seconds() const {
    if (clock.empty()) {
        return -1;
    }
    size_t colon = clock.find(':');
    return std::stoi(clock.substr(0, colon)) * 60 + std::stoi(clock.substr(colon + 1));
}

StatusLine status_line(std::string_view line) {
    StatusLine out;
    for (char c : line) {
        if (c == ' ' && (out.words.empty() || out.words.back() == ' ')) {
            continue;
        }
        out.words += c;
    }
    if (!out.words.empty() && out.words.back() == ' ') {
        out.words.pop_back();
    }
    size_t space = out.words.rfind(' ');
    size_t start = space == std::string::npos ? 0 : space + 1;
    if (is_clock(std::string_view(out.words).substr(start))) {
        out.clock = out.words.substr(start);
        out.words.erase(space == std::string::npos ? 0 : space);
    }
    return out;
}

unsigned WaitStatus::moment(const StatusLine& line, unsigned wait_ms) {
    if (wait_ms == 0) {
        return 0;
    }
    /* The line's own clock where it shows one, so a reminder never says the
     * second before its moment. */
    int seconds = line.seconds();
    return reminder_moment(seconds >= 0 ? static_cast<unsigned>(seconds) : wait_ms / 1000);
}

std::string WaitStatus::open(std::string_view line, unsigned wait_ms) {
    StatusLine now = status_line(line);
    m_words = now.words;
    m_moment = moment(now, wait_ms);
    return now.shown();
}

std::string WaitStatus::frame(std::string_view line, unsigned wait_ms) {
    StatusLine now = status_line(line);
    unsigned reached = moment(now, wait_ms);
    bool changed = now.words != m_words;
    bool reminder = reached > m_moment;
    m_words = now.words;
    /* A change says the line, which serves for a moment on the same frame.
     * Frames that do not wait leave the count where the wait left it. */
    if (wait_ms > 0) {
        m_moment = reached;
    }
    return changed || reminder ? now.shown() : std::string();
}

}  // namespace a11y
