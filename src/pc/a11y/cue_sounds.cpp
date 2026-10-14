/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cue_sounds.hpp"
#include "miniaudio.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace a11y {

Sound read_sound(ma_data_source* source, int frames) {
    Sound sound;
    sound.samples.resize(static_cast<size_t>(std::max(frames, 0)));
    ma_uint64 read = 0;
    if (ma_data_source_read_pcm_frames(source, sound.samples.data(), sound.samples.size(), &read) !=
        MA_SUCCESS)
    {
        read = 0;
    }
    sound.samples.resize(static_cast<size_t>(read));
    return sound;
}

namespace {

/* Raised-cosine gain i frames into an edge of edge frames: 0 to nearly 1. */
float edge_gain(int i, int edge) {
    return 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * static_cast<float>(i) /
                                  static_cast<float>(edge));
}

void fade_in_frames(Sound& sound, int edge) {
    for (int i = 0; i < edge; i++) {
        sound.samples[static_cast<size_t>(i)] *= edge_gain(i, edge);
    }
}

void fade_out_frames(Sound& sound, int edge) {
    const int size = static_cast<int>(sound.samples.size());
    for (int i = 0; i < edge; i++) {
        sound.samples[static_cast<size_t>(size - 1 - i)] *= edge_gain(i, edge);
    }
}

}  // namespace

void shape_edges(Sound& sound, float seconds) {
    const int size = static_cast<int>(sound.samples.size());
    const int edge = std::min(static_cast<int>(seconds * kCueRate), size / 2);
    fade_in_frames(sound, edge);
    fade_out_frames(sound, edge);
}

void fade_in(Sound& sound, float seconds) {
    const int size = static_cast<int>(sound.samples.size());
    fade_in_frames(sound, std::min(static_cast<int>(seconds * kCueRate), size));
}

void fade_out(Sound& sound, float seconds) {
    const int size = static_cast<int>(sound.samples.size());
    fade_out_frames(sound, std::min(static_cast<int>(seconds * kCueRate), size));
}

void lead_in(Sound& sound, float seconds) {
    const int frames = std::max(static_cast<int>(seconds * kCueRate), 0);
    sound.samples.insert(sound.samples.begin(), static_cast<size_t>(frames), 0.0f);
}

}  // namespace a11y
