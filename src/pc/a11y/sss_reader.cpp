/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sss_reader.hpp"
#include "pc/pc.h"
#include "speech.hpp"

namespace a11y {

void SssReader::forget() {
    m_speech.forget();
    m_steerer.forget();
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
    m_speech.frame(state);
}

}  // namespace a11y
