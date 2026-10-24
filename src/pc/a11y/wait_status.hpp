/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A wait's status line read aloud: Direct Connect's call ("Calling
 * RYAN#K3X-Q2M-7A  0:12"), and later Unranked's search ("Looking for an
 * opponent  0:12"), whose lines end with the wait's clock while it waits.
 *
 * The clock ticking alone is no change, so lines are compared without it. So
 * that a long wait never sounds like a hang, the current line is said again
 * at a few moments timed from the wait's start (reminder_moment), with its
 * clock as shown; a line without a clock ("Found them but couldn't connect.
 * Still trying...") is said again at the same moments, timed by the wait's
 * own clock, which the line then does not show. */
#pragma once
#include <string>
#include <string_view>

namespace a11y {

/* The line with runs of spaces, the lobby screens' padding and spacing, made
 * one, and none at either end. */
std::string spaces_collapsed(std::string_view line);

/* A status line split: its words, runs of spaces made one, and the clock at
 * its end ("0:12"), empty for a line without one. */
struct StatusLine {
    std::string words;
    std::string clock;

    /* The line as shown, its spaces collapsed. */
    std::string shown() const;
    /* The clock in seconds, or -1 without one. */
    int seconds() const;
};

StatusLine status_line(std::string_view line);

class WaitStatus {
public:
    /* The first line of a wait: what to say, the line as shown. wait_ms is
     * the wait's clock, 0 when it is not waiting (connecting, starting or
     * failed), which says no reminders. */
    std::string open(std::string_view line, unsigned wait_ms);

    /* A later frame's line: the line as shown when its words changed, a
     * reminder when a moment has come since the last thing said, else
     * empty. */
    std::string frame(std::string_view line, unsigned wait_ms);

private:
    /* The moment reached by this line's clock, or by wait_ms without one. */
    static unsigned moment(const StatusLine& line, unsigned wait_ms);

    std::string m_words;
    unsigned m_moment = 0;
};

}  // namespace a11y
