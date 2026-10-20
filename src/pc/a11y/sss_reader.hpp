/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage select as the fork follows it: what the frame hook reports, the
 * speech, and steering the stage cursor with the D-pad
 * (.scratch/stage-select/spec.md). It holds everything the fork keeps about
 * the screen, so a scene change forgets it all in one call. It reads the
 * game through game_access.h and writes nothing but the stick it hands the
 * pad hook. */
#pragma once
#include "game_access.h"
#include "sss_speech.hpp"
#include "steerer.hpp"

namespace a11y {

class Speech;

class SssReader {
public:
    /* steer: the D-pad steers the stage cursor. speech must outlive this. */
    SssReader(Speech& speech, bool steer)
        : m_speech(speech), m_steer(steer),
          m_steerer(kStageCursorMovement,
              [this](const int& tile) { return m_speech.tile_words(m_state, tile); }) {}

    /* A scene was entered. */
    void forget();
    /* The frame hook: the screen this frame, and whether the session is
     * online. */
    void frame(const A11ySssScreen& screen, bool online);
    /* The cursor hook, once per simulated frame while no pick is made: the
     * stage cursor at `at` after this frame's movement, moved by the stick
     * (stick_x, stick_y) with the dead zone taken off. Steps, and the
     * glide's next frame. */
    void cursor(Point at, int stick_x, int stick_y);
    /* The resolve hook: online, the stage the coin flip chose between the
     * two picks. */
    void resolved(int stage_tile, int local_pick, int remote_pick);
    /* The pad hook: the stick steering asks for, handed over once; false
     * when it asks for none, so the stick returns to rest by itself. */
    bool take_stick(Stick* out) { return m_steerer.take_stick(out); }
    /* Whether the D-pad steers the stage cursor, rather than doing what the
     * base port would make of it. */
    bool steers() const { return m_steer; }
    /* The Steering switch, live: off mid-glide, the glide ends there and
     * the stick goes back to rest at the next publish. */
    void set_steering(bool on);

private:
    void press(Direction direction, Point at);

    SssSpeech m_speech;
    bool m_steer;
    /* The table's layout was found changed and logged. */
    bool m_layout_logged = false;
    /* This frame's snapshot, from the frame hook. */
    A11ySssState m_state{};
    bool m_have_state = false;
    /* The row the last step was taken along, and the tile it named: Left
     * and Right from a tall tile go back along that row. */
    int m_row = -1;
    int m_row_tile = A11Y_SSS_NO_TILE;
    /* The glide to a tile, by its index in the game's table. */
    Steerer<int> m_steerer;
};

}  // namespace a11y
