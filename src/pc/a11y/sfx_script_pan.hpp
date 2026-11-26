/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The pan a placed sound's own script asks for (primer docs/a11y/cues.md,
 * "A sound's own script"). A sound's script may set an absolute pan of its
 * own (a pan command in AXDriverInterp, axdriver.c), near the centre, and
 * the engine applies it as the sound begins, which would pull a placed
 * sound back to the middle for its opening. For a voice placed here, that
 * pan is taken as an offset from the voice's place instead, so the
 * sound's own small nudge survives and it stays where its fighter is.
 *
 * Threads: the game thread remembers each voice's place as it places or
 * moves it; the engine's audio thread asks, from the script. One atomic
 * word per engine voice slot, nothing locked. */
#pragma once
#include <cstdint>

namespace a11y {

/* Game thread: the voice (the engine's id) is placed at pan (0 left, 0x80
 * centre, 0xFF right). */
void sfx_remember_pan(int voice, std::uint8_t pan);

/* Any thread: the pan for a voice whose script asks for script_pan, an
 * absolute pan in the same units: unchanged for a voice not placed here,
 * otherwise its place moved by the script's distance from the centre. */
std::uint8_t sfx_script_pan(int voice, int script_pan);

/* Tests: every voice forgotten. */
void sfx_forget_pans();

}  // namespace a11y
