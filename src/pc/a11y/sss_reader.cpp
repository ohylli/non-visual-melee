/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sss_reader.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include "sss_targets.hpp"

namespace a11y {

void SssReader::forget() {
    m_speech.forget();
    m_steerer.forget();
    m_have_state = false;
    m_row = -1;
    m_row_tile = A11Y_SSS_NO_TILE;
}

void SssReader::frame(const A11ySssScreen& screen, bool online) {
    A11ySssState state;
    if (!a11y_game_sss_state(&screen, &state)) {
        /* A decomp sync changed the table's rows: silent rather than read
         * the wrong fields. */
        if (!m_layout_logged && log_enabled()) {
            pc_log_line("[a11y] stage select: the table of tiles has rows of %d bytes, not the "
                        "layout game_access.c knows; silent",
                screen.tile_size);
        }
        m_layout_logged = true;
        return;
    }
    state.online = online;
    m_state = state;
    m_have_state = true;
    if (m_steerer.active()) {
        if (!may_steer(state)) {
            m_steerer.stop("steering is off here", false);
        } else {
            /* The tiles slide into place as the screen opens. */
            m_steerer.retarget(aim_point(state, m_steerer.status().destination));
        }
    }
    m_speech.frame(state, m_steerer.status());
    m_steerer.reported();
}

void SssReader::cursor(Point at, int stick_x, int stick_y) {
    if (!m_steer || !m_have_state) {
        return;
    }
    bool allowed = may_steer(m_state);
    if (!allowed && m_steerer.active()) {
        m_steerer.stop("steering is off here", false);
    }
    A11yPad pad;
    a11y_game_pad(steering_port(m_state), &pad);
    Direction direction;
    if (allowed && pressed_direction(pad, &direction)) {
        press(direction, at);
    }
    /* Offline the stick may be another controller's, pushed while
     * controller 1's rests: the glide takes it for the player's own. */
    m_steerer.frame(at, stick_as_held(stick_x, stick_y));
}

void SssReader::resolved(int stage_tile, int local_pick, int remote_pick) {
    if (m_have_state) {
        m_speech.resolved(m_state, stage_tile, local_pick, remote_pick);
    }
}

void SssReader::press(Direction direction, Point at) {
    /* A press during a glide steps on from where it was going. */
    int from = m_steerer.active() ? m_steerer.status().destination : tile_at(m_state, at);
    TileStep to = step(m_state, from, at, direction, from == m_row_tile ? m_row : -1);
    m_speech.step(m_state, to.tile);
    m_row = to.row;
    m_row_tile = to.tile;
    if (to.moves) {
        m_steerer.start(to.tile, aim_point(m_state, to.tile));
    }
}

}  // namespace a11y
