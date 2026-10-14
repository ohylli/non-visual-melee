/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Making cue sounds, at engine start: a cue's sound is a mono buffer
 * (Sound, cue_engine.hpp), filled here by code or from any miniaudio data
 * source, a decoded file later included. */
#pragma once
#include "cue_engine.hpp"

/* miniaudio.h's own declaration, so including this header does not pull in
 * all of miniaudio. */
typedef void ma_data_source;

namespace a11y {

/* Up to frames of a data source that gives 32-bit float mono at kCueRate (a
 * waveform, noise, a decoder set to that format); fewer if it ends first. */
Sound read_sound(ma_data_source* source, int frames);

/* Raised-cosine edges of the given length at both ends, so the sound starts
 * and ends in silence. */
void shape_edges(Sound& sound, float seconds);

/* A raised-cosine edge of the given length at the start alone, for a soft
 * attack. */
void fade_in(Sound& sound, float seconds);

/* A raised-cosine edge of the given length at the end alone, so the sound
 * ends in silence. */
void fade_out(Sound& sound, float seconds);

/* Silence of the given length before the sound: room for the start ramp, so
 * it does not eat the sound's attack. */
void lead_in(Sound& sound, float seconds);

}  // namespace a11y
