/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage select as the fork follows it: what the frame hook reports, and the
 * speech (.scratch/stage-select/spec.md). It holds everything the fork keeps
 * about the screen, so a scene change forgets it all in one call. It reads
 * the game through game_access.h and writes nothing. */
#pragma once
#include "game_access.h"
#include "sss_speech.hpp"

namespace a11y {

class Speech;

class SssReader {
public:
    /* speech must outlive this. */
    explicit SssReader(Speech& speech) : m_speech(speech) {}

    /* A scene was entered. */
    void forget();
    /* The frame hook: the screen this frame, and whether the session is
     * online. */
    void frame(const A11ySssScreen& screen, bool online);

private:
    SssSpeech m_speech;
    /* The table's layout was found changed and logged. */
    bool m_layout_logged = false;
};

}  // namespace a11y
