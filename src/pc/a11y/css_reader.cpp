/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_reader.hpp"
#include <algorithm>

namespace a11y {

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
    m_steerer.forget();
}

void CssReader::frame(const A11yCssScreen& screen, int local_port, bool online) {
    A11yCssState state;
    a11y_game_css_state(&screen, m_reports, m_coins, local_port, &state);
    state.online = online;
    if (m_steerer.active()) {
        if (!m_local_updated) {
            /* A hand whose controller is unplugged stops updating. */
            m_steerer.stop("the hand stopped updating", true);
        } else if (!may_steer(state)) {
            m_steerer.stop("steering is off here", false);
        } else {
            follow_destination(state);
        }
    }
    m_local_updated = false;
    m_state = state;
    m_have_state = true;
    m_speech.frame(state, m_steerer.status());
    m_steerer.reported();
}

void CssReader::set_steering(bool on) {
    if (!on && m_steer) {
        m_steerer.switch_off("steering was switched off");
    }
    m_steer = on;
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
    Target destination = m_steerer.status().destination;
    bool holding = state.hand.slider != A11Y_CSS_NO_SLIDER;
    if (destination.level.has_value() != holding) {
        /* A glide to a value of a slider let go of would drag a free hand,
         * one to a target would drag a slider just grabbed. */
        m_steerer.stop(holding ? "a slider was grabbed" : "the slider was let go", false);
        return;
    }
    /* A knob moves with its value and a resting coin drifts. A target gone
     * from the rows (a slot closed, say) is aimed at where it was. */
    std::vector<std::vector<Target>> rows = target_rows(state);
    bool exists = destination.level || std::any_of(rows.begin(), rows.end(), [&](const auto& row) {
        return std::find(row.begin(), row.end(), destination) != row.end();
    });
    if (exists) {
        m_steerer.retarget(aim_point(state, destination));
    }
}

void CssReader::steer(const A11yCssHandReport& report) {
    m_state.hand.x = report.x;
    m_state.hand.y = report.y;
    Point at{report.x, report.y};
    A11yPad pad;
    a11y_game_pad(m_state.local_port, &pad);
    bool allowed = may_steer(m_state);
    if (!allowed && m_steerer.active()) {
        m_steerer.stop("steering is off here", false);
    }
    Direction direction;
    if (allowed && pressed_direction(pad, &direction)) {
        press(direction, at);
    }
    m_steerer.frame(at, Stick{pad.stick_x, pad.stick_y});
}

void CssReader::press(Direction direction, Point hand) {
    /* A press during a glide steps on from where it was going. */
    Target from =
        m_steerer.active() ? m_steerer.status().destination : locate(m_state, hand.x, hand.y);
    Target to = step(m_state, from, hand.x, hand.y, direction);
    m_speech.step(m_state, to);
    /* At an edge the step names where the hand is, or is going. */
    if (to.kind == TargetKind::none || to == from) {
        return;
    }
    m_steerer.start(to, aim_point(m_state, to));
}

}  // namespace a11y
