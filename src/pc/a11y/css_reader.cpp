/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_reader.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <algorithm>
#include <string>

namespace a11y {
namespace {

/* The first direction pressed this frame, if any. */
bool pressed(const A11yPad& pad, Direction* out) {
    if (pad.pressed_left) {
        *out = Direction::left;
    } else if (pad.pressed_right) {
        *out = Direction::right;
    } else if (pad.pressed_up) {
        *out = Direction::up;
    } else if (pad.pressed_down) {
        *out = Direction::down;
    } else {
        return false;
    }
    return true;
}

/* Why the glide ended, for the log. */
const char* why_ended(GlideEnd end) {
    switch (end) {
    case GlideEnd::failed:
        return "could not get there";
    case GlideEnd::abandoned:
        return "the player's stick took over";
    case GlideEnd::none:
    case GlideEnd::arrived:
        break;
    }
    return "arrived";
}

}  // namespace

void CssReader::forget() {
    m_speech.forget();
    for (A11yCssHandReport& report : m_reports) {
        report = A11yCssHandReport{};
    }
    for (A11yCssCoinReport& coin : m_coins) {
        coin = A11yCssCoinReport{};
    }
    m_have_state = false;
    m_local_updated = false;
    m_glide = Glide{};
    m_glide_status = GlideStatus{};
    m_request_waiting = false;
}

void CssReader::frame(const A11yCssScreen& screen, int local_port, bool online) {
    A11yCssState state;
    a11y_game_css_state(&screen, m_reports, m_coins, local_port, &state);
    state.online = online;
    if (m_glide.active()) {
        if (!m_local_updated) {
            /* A hand whose controller is unplugged stops updating. */
            stop_glide("the hand stopped updating", true);
        } else if (!may_steer(state)) {
            stop_glide("steering is off here", false);
        } else {
            follow_destination(state);
        }
    }
    m_local_updated = false;
    m_state = state;
    m_have_state = true;
    m_speech.frame(state, m_glide_status);
    if (m_glide_status.phase != GlideStatus::Phase::gliding) {
        m_glide_status = GlideStatus{};
    }
}

void CssReader::hand(int hand, const A11yCssHandReport& report) {
    m_reports[hand] = report;
    if (m_steer && m_have_state && hand == m_state.local_slot) {
        m_local_updated = true;
        steer(report);
    }
}

void CssReader::coin(int slot, float x, float y) {
    m_coins[slot] = A11yCssCoinReport{true, x, y};
}

bool CssReader::take_stick(Stick* out) {
    m_glide.published();
    if (!m_request_waiting) {
        return false;
    }
    m_request_waiting = false;
    *out = m_request;
    return true;
}

bool CssReader::may_steer(const A11yCssState& state) const {
    /* Steering writes controller 1's pad only, which in a single-player mode
     * moves the hand only if controller 1 started it; online, the local
     * player's pad is controller 1's, whoever they are in the game. The name
     * tag window keeps the hand inside itself. */
    bool reachable = !single_player(state) || state.local_port == 0;
    return reachable && state.exit == A11Y_CSS_STAYING && state.hand.present &&
           !state.slots[state.local_slot].name_tags_open;
}

void CssReader::follow_destination(const A11yCssState& state) {
    Target destination = m_glide_status.destination;
    bool holding = state.hand.slider != A11Y_CSS_NO_SLIDER;
    if (destination.level.has_value() != holding) {
        /* A glide to a value of a slider let go of would drag a free hand,
         * one to a target would drag a slider just grabbed. */
        stop_glide(holding ? "a slider was grabbed" : "the slider was let go", false);
        return;
    }
    /* A knob moves with its value and a resting coin drifts. A target gone
     * from the rows (a slot closed, say) is aimed at where it was. */
    std::vector<std::vector<Target>> rows = target_rows(state);
    bool exists = destination.level || std::any_of(rows.begin(), rows.end(), [&](const auto& row) {
        return std::find(row.begin(), row.end(), destination) != row.end();
    });
    if (exists) {
        m_glide.retarget(aim_point(state, destination));
    }
}

void CssReader::steer(const A11yCssHandReport& report) {
    m_state.hand.x = report.x;
    m_state.hand.y = report.y;
    Point at{report.x, report.y};
    A11yPad pad;
    a11y_game_pad(m_state.local_port, &pad);
    bool allowed = may_steer(m_state);
    if (!allowed && m_glide.active()) {
        stop_glide("steering is off here", false);
    }
    Direction direction;
    if (allowed && pressed(pad, &direction)) {
        press(direction, at);
    }
    /* Every frame, gliding or not, so the glide keeps count of the sticks
     * still on their way. */
    GlideFrame glide = m_glide.frame(at, Stick{pad.stick_x, pad.stick_y});
    m_request = glide.stick;
    m_request_waiting = moves(glide.stick);
    if (glide.end != GlideEnd::none) {
        glide_ended(glide.end == GlideEnd::failed, why_ended(glide.end));
    }
}

void CssReader::press(Direction direction, Point hand) {
    /* A press during a glide steps on from where it was going. */
    Target from = m_glide.active() ? m_glide_status.destination : locate(m_state, hand.x, hand.y);
    Target to = step(m_state, from, hand.x, hand.y, direction);
    m_speech.step(m_state, to);
    /* At an edge the step names where the hand is, or is going. */
    if (to.kind == TargetKind::none || to == from) {
        return;
    }
    m_glide_status = GlideStatus{GlideStatus::Phase::gliding, to};
    Point aim = aim_point(m_state, to);
    m_glide.start(aim);
    if (log_enabled()) {
        pc_log_line("[a11y] glide to \"%s\" at (%.2f, %.2f)",
            m_speech.target_words(m_state, to).c_str(), aim.x, aim.y);
    }
}

void CssReader::glide_ended(bool failed, const char* why) {
    if (log_enabled()) {
        pc_log_line("[a11y] glide to \"%s\" ended after %d frames: %s",
            m_speech.target_words(m_state, m_glide_status.destination).c_str(), m_glide.frames(),
            why);
    }
    m_glide_status.phase = failed ? GlideStatus::Phase::failed : GlideStatus::Phase::ended;
}

void CssReader::stop_glide(const char* why, bool failed) {
    m_glide.stop();
    glide_ended(failed, why);
}

}  // namespace a11y
