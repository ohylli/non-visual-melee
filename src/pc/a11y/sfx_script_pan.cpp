/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sfx_script_pan.hpp"
#include <algorithm>
#include <array>
#include <atomic>

namespace a11y {

namespace {

/* The engine's voice id is its slot in the low 7 bits (axdriver.c:
 * vidhigh << 7 | slot) and a count above them that makes it unique. */
constexpr int kSlots = 0x80;

/* Per slot: the voice id plus one, shifted up, and its pan in the low
 * byte; 0 for none. The id is kept whole, so a later voice in the same
 * slot never takes an earlier one's place. */
std::array<std::atomic<std::uint64_t>, kSlots> s_pans{};

constexpr std::uint8_t kCentre = 0x80;

}  // namespace

void sfx_remember_pan(int voice, std::uint8_t pan) {
    if (voice < 0) {
        return;
    }
    const std::uint64_t word = ((static_cast<std::uint64_t>(voice) + 1) << 8) | pan;
    s_pans[static_cast<std::size_t>(voice & (kSlots - 1))].store(word, std::memory_order_relaxed);
}

std::uint8_t sfx_script_pan(int voice, int script_pan) {
    const std::uint8_t asked = static_cast<std::uint8_t>(std::clamp(script_pan, 0, 0xFF));
    if (voice < 0) {
        return asked;
    }
    const std::uint64_t word =
        s_pans[static_cast<std::size_t>(voice & (kSlots - 1))].load(std::memory_order_relaxed);
    if ((word >> 8) != static_cast<std::uint64_t>(voice) + 1) {
        return asked;
    }
    const int place = static_cast<int>(word & 0xFF);
    return static_cast<std::uint8_t>(std::clamp(place + (asked - kCentre), 0, 0xFF));
}

void sfx_forget_pans() {
    for (auto& word : s_pans) {
        word.store(0, std::memory_order_relaxed);
    }
}

}  // namespace a11y
