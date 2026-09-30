/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_reader.hpp"
#include "pc/pc.h"
#include "speech.hpp"
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
    m_have_state = false;
    m_local_updated = false;
    m_glide = Glide{};
    m_glide_status = GlideStatus{};
    m_request_waiting = false;
}

void CssReader::frame(const A11yCssScreen& screen, int local_port, bool online) {
    A11yCssState state;
    a11y_game_css_state(&screen, m_reports, local_port, &state);
    state.online = online;
    if (m_glide.active()) {
        if (!m_local_updated) {
            /* A hand whose controller is unplugged stops updating. */
            stop_glide("the hand stopped updating", true);
        } else if (!may_steer(state)) {
            stop_glide("steering is off here", false);
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
        steer(hand, report);
    }
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
    /* VS modes only so far: single-player modes read their one hand from
     * the port that started the mode. A held slider follows the hand, and
     * the name tag window keeps it inside itself. */
    return state.hand_count == A11Y_CSS_SLOTS && state.exit == A11Y_CSS_STAYING &&
           state.hand.present && state.hand.slider == A11Y_CSS_NO_SLIDER &&
           !state.slots[state.local_slot].name_tags_open;
}

void CssReader::steer(int hand, const A11yCssHandReport& report) {
    m_state.hand.x = report.x;
    m_state.hand.y = report.y;
    Point at{report.x, report.y};
    A11yPad pad;
    a11y_game_pad(hand, &pad);
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
    Target from =
        m_glide.active() ? m_glide_status.destination : target_at(m_state, hand.x, hand.y);
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
