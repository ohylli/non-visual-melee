/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cue_sounds.hpp"
#include "miniaudio.h"
#include <algorithm>
#include <cmath>

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

void shape_edges(Sound& sound, float seconds) {
    const int size = static_cast<int>(sound.samples.size());
    const int edge = std::min(static_cast<int>(seconds * kCueRate), size / 2);
    for (int i = 0; i < edge; i++) {
        const float gain =
            0.5f - 0.5f * std::cos(3.14159265f * static_cast<float>(i) / static_cast<float>(edge));
        sound.samples[static_cast<size_t>(i)] *= gain;
        sound.samples[static_cast<size_t>(size - 1 - i)] *= gain;
    }
}

void lead_in(Sound& sound, float seconds) {
    const int frames = std::max(static_cast<int>(seconds * kCueRate), 0);
    sound.samples.insert(sound.samples.begin(), static_cast<size_t>(frames), 0.0f);
}

}  // namespace a11y
