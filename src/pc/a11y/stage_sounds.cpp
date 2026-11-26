/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "stage_sounds.hpp"
#include "pc/pc.h"
#include <algorithm>
#include <array>
#include <string>

namespace a11y {

namespace {

/* lbaudio_ax.c's ssm_files from castle.ssm to pstadium.ssm. */
constexpr std::array<std::string_view, kLastStageBank - kFirstStageBank + 1> kStageBanks{"castle",
    "corneria", "greatbay", "kongo", "mutecity", "onett", "zebes", "garden", "klaid", "greens",
    "venom", "bigblue", "fourside", "pupupu", "pstadium"};

/* The engine's voice pool: no more can play. */
constexpr std::size_t kMostMuted = 0x60;

}  // namespace

bool is_stage_bank(int bank) {
    return bank >= kFirstStageBank && bank <= kLastStageBank;
}

std::string_view stage_bank_name(int bank) {
    return is_stage_bank(bank) ? kStageBanks[static_cast<std::size_t>(bank - kFirstStageBank)] :
                                 std::string_view{};
}

bool StageSounds::started(bool mute, int voice, int bank, std::uint8_t* volume) {
    if (!mute || !is_stage_bank(bank)) {
        return false;
    }
    const std::string name(stage_bank_name(bank));
    pc_log_line("[a11y] sfx muted: stage %s volume %d", name.c_str(), *volume);
    if (m_voices.size() < kMostMuted) {
        m_voices.push_back(Muted{voice, *volume});
    }
    *volume = 0;
    return true;
}

void StageSounds::tick(bool mute) {
    if (!mute) {
        for (const Muted& m : m_voices) {
            m_set_volume(m.voice, m.volume);
        }
        m_voices.clear();
        return;
    }
    /* Held at 0: the stage's own code may have faded it up since. */
    std::erase_if(m_voices, [&](const Muted& m) { return !m_set_volume(m.voice, 0); });
}

}  // namespace a11y
