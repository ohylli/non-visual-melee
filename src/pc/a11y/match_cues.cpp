/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "match_cues.hpp"
#include "cue_sounds.hpp"
#include "miniaudio.h"
#include <cmath>

namespace a11y {
namespace {

/* A short click lifted under its volume to sit level with held tones. */
constexpr float kYourBoost = 1.5f;

/* The tick of your position cue, the Star Fox 64 fork's aim click: a
 * 1500 Hz sine decaying fast over 20 ms, its last 3 ms faded to silence,
 * after 12 ms of silence so the start ramp leaves its attack alone. 32 ms
 * in all, 64 ms an octave down. */
Sound tick_sound() {
    constexpr double kHz = 1500.0;
    constexpr float kSeconds = 0.02f;
    constexpr float kDecayPerSecond = 150.0f;
    constexpr float kAmplitude = 0.95f;
    constexpr float kTailSeconds = 0.003f;
    constexpr float kLeadInSeconds = 0.012f;
    ma_waveform_config config =
        ma_waveform_config_init(ma_format_f32, 1, kCueRate, ma_waveform_type_sine, kAmplitude, kHz);
    ma_waveform wave;
    Sound sound;
    if (ma_waveform_init(&config, &wave) == MA_SUCCESS) {
        sound = read_sound(&wave, static_cast<int>(kSeconds * kCueRate));
        ma_waveform_uninit(&wave);
    }
    const int size = static_cast<int>(sound.samples.size());
    const int tail = static_cast<int>(kTailSeconds * kCueRate);
    for (int i = 0; i < size; i++) {
        float gain = std::exp(-kDecayPerSecond * static_cast<float>(i) / kCueRate);
        int remaining = size - 1 - i;
        if (remaining < tail) {
            gain *= 0.5f - 0.5f * std::cos(3.14159265f * static_cast<float>(remaining) /
                                           static_cast<float>(tail));
        }
        sound.samples[static_cast<size_t>(i)] *= gain;
    }
    lead_in(sound, kLeadInSeconds);
    return sound;
}

}  // namespace

PositionMapping MatchCues::PositionValues::mapping() const {
    return PositionMapping{edge_pan.value(), up_range.value(), down_range.value(),
        slow_interval.value(), fast_interval.value(), far_distance.value(), near_distance.value()};
}

MatchCues::PositionValues MatchCues::add_position_values(SettingsGroup group) {
    const PositionMapping defaults;
    return PositionValues{
        group.add_number(NumberSpec{"edge_pan", "Edge pan",
            "How far to the side the main floor's ends sound; past them the pan goes on to fully "
            "left or right at the side blast zones.",
            "", defaults.edge_pan, 0.0f, 1.0f, 0.05f}),
        group.add_number(NumberSpec{"up_range", "Up range",
            "How many octaves higher the pitch is at the top of the camera's view than on the "
            "main floor.",
            "", defaults.up_range, 0.0f, 2.0f, 0.1f}),
        group.add_number(NumberSpec{"down_range", "Down range",
            "How many octaves lower the pitch is at the bottom blast zone than on the main "
            "floor.",
            "", defaults.down_range, 0.0f, kWidestDownRange, 0.1f}),
        group.add_number(NumberSpec{"slow_interval", "Slow interval",
            "Time between ticks with the opponent at the far distance or beyond, or with no "
            "opponent.",
            "s", defaults.slow_interval, 0.1f, 3.0f, 0.05f}),
        group.add_number(NumberSpec{"fast_interval", "Fast interval",
            "Time between ticks with the opponent at the near distance or closer.", "s",
            defaults.fast_interval, kFastestInterval, 1.0f, 0.01f}),
        group.add_number(NumberSpec{"far_distance", "Far distance",
            "From this distance to the opponent and beyond, the ticks are slow. Final "
            "Destination's main floor is about 171 units wide.",
            "units", defaults.far_distance, 10.0f, 400.0f, 5.0f}),
        group.add_number(NumberSpec{"near_distance", "Near distance",
            "From this distance to the opponent and closer, the ticks are fast.", "units",
            defaults.near_distance, 0.0f, 100.0f, 1.0f}),
    };
}

MatchCues::MatchCues(CueSettings& settings, CueEngine& engine)
    : MatchCues(settings.add_cue("your_position", "Your position cue", kYourBoost), engine) {}

MatchCues::MatchCues(const CueGroup& your, CueEngine& engine)
    : m_engine(engine), m_your(add_position_values(your.group)) {
    VoiceSpec spec;
    spec.name = "your position";
    spec.cue = your.values.cue;
    spec.sound = tick_sound();
    spec.mode = VoiceMode::pulsed;
    spec.fastest_interval = kFastestInterval;
    spec.lowest_pitch = std::exp2(-kWidestDownRange);
    m_your_voice = m_engine.add_voice(std::move(spec));
}

void MatchCues::tick(const A11yMatchState& match) {
    if (m_your_voice >= 0) {
        std::optional<VoiceTargets> targets = your_position(match, m_your.mapping());
        if (targets.has_value()) {
            m_engine.start(m_your_voice, *targets);
        } else if (m_engine.playing(m_your_voice)) {
            m_engine.stop(m_your_voice);
        }
    }
    if (match.running) {
        m_engine.trace_tick(m_tick++);
    }
}

}  // namespace a11y
