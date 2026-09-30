/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select as the fork follows it: what the hooks report, the speech,
 * and steering the local player's hand with the D-pad
 * (.scratch/character-select/spec.md, "Stage 2: stepping"). It holds
 * everything the fork keeps about the screen, so a scene change forgets it
 * all in one call. It reads the game through game_access.h and writes
 * nothing but the stick it hands the pad hook; the parts it drives (the
 * speech, the targets and the glide) are pure. */
#pragma once
#include "css_speech.hpp"
#include "css_targets.hpp"
#include "game_access.h"
#include "steering.hpp"

namespace a11y {

class Speech;

class CssReader {
public:
    /* steer: the D-pad steers the hand. speech must outlive this. */
    CssReader(Speech& speech, bool steer) : m_speech(speech), m_steer(steer) {}

    /* A scene was entered. */
    void forget();
    /* The frame hook: the screen this frame, the port the local player
     * drives, and whether the session is online. */
    void frame(const A11yCssScreen& screen, int local_port, bool online);
    /* The hand hook: hand has updated this simulated frame. */
    void hand(int hand, const A11yCssHandReport& report);
    /* The coin hook: the coin of slot is at (x, y). */
    void coin(int slot, float x, float y);
    /* The pad hook: the stick steering asks for, handed over once; false
     * when it asks for none, so the stick returns to rest by itself. */
    bool take_stick(Stick* out);

private:
    bool may_steer(const A11yCssState& state) const;
    /* A glide under way follows its destination as it moves, and stops
     * where holding a slider or not changes what a step means. */
    void follow_destination(const A11yCssState& state);
    /* The local hand updated: steps, and the glide's next frame. */
    void steer(int hand, const A11yCssHandReport& report);
    void press(Direction direction, Point hand);
    /* The glide ended, failed or not, for why: logged now, spoken on the
     * next frame. */
    void glide_ended(bool failed, const char* why);
    /* Stops a glide under way, with no announcement unless failed. */
    void stop_glide(const char* why, bool failed);

    CssSpeech m_speech;
    bool m_steer;
    /* The hands as the hand hook last reported them; a hand not reported
     * since the scene was entered reads as unseen. */
    A11yCssHandReport m_reports[A11Y_CSS_SLOTS] = {};
    /* The coins as the coin hook last reported them, indexed by slot. */
    A11yCssCoinReport m_coins[A11Y_CSS_SLOTS] = {};
    /* This frame's snapshot, from the frame hook; the local hand's hook
     * moves its hand to where it went this frame. */
    A11yCssState m_state{};
    bool m_have_state = false;
    /* The local hand reported since the last frame hook. */
    bool m_local_updated = false;
    Glide m_glide;
    /* Where the glide goes, and how it ended until the frame hook says so. */
    GlideStatus m_glide_status;
    /* The stick the glide asks for, until the pad hook takes it. */
    Stick m_request;
    bool m_request_waiting = false;
};

}  // namespace a11y
