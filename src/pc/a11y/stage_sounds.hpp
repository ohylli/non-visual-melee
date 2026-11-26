/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage sounds (primer docs/a11y/cues.md, "Stage sounds"): a switch that
 * mutes the sounds a stage makes of its own, its traffic, its Arwings, its
 * crowd, so a blind player hears the fighters more clearly. A stage's
 * sounds come from that stage's own sound bank (castle.ssm to
 * pstadium.ssm, lbaudio_ax.c's ssm_files), so the bank a sound id falls in
 * says whose it is. A muted sound plays at volume 0, and is held there
 * each tick, since the stage's own code fades its long sounds in and out;
 * switched back on, each one still playing gets its volume back. Only the
 * game's sound engine is touched, never the simulation. Game thread only. */
#pragma once
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace a11y {

/* The stages' banks, by their index in the game's bank list. */
constexpr int kFirstStageBank = 34; /* castle.ssm */
constexpr int kLastStageBank = 48;  /* pstadium.ssm */

bool is_stage_bank(int bank);

/* The bank's file name without .ssm ("corneria"), "" for none known. */
std::string_view stage_bank_name(int bank);

class StageSounds {
public:
    /* Sets a voice's volume in the game's engine; false once it has
     * ended. */
    using SetVolume = std::function<bool(int voice, std::uint8_t volume)>;

    explicit StageSounds(SetVolume set_volume) : m_set_volume(std::move(set_volume)) {}

    /* A sound's voice has started (pc_a11y_sfx_started), before it first
     * sounds, from the given bank. With mute, a stage's sound is started at
     * volume 0 and held there; true when it was muted. */
    bool started(bool mute, int voice, int bank, std::uint8_t* volume);

    /* Once a tick: each muted voice held at 0, or with mute off given its
     * volume back and let go. A voice that has ended is let go. */
    void tick(bool mute);

    std::size_t muted() const { return m_voices.size(); }

private:
    struct Muted {
        int voice;
        std::uint8_t volume; /* its own, as it started */
    };

    SetVolume m_set_volume;
    std::vector<Muted> m_voices;
};

}  // namespace a11y
