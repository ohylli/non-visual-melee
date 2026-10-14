/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "match_cues.hpp"
#include "cue_sounds.hpp"
#include "miniaudio.h"
#include <cmath>
#include <numbers>
#include <utility>

namespace a11y {
namespace {

/* A short click lifted under its volume to sit level with held tones. */
constexpr float kYourBoost = 2.0f;
/* The opponent's lower pulse more, as tuned by ear. */
constexpr float kOpponentBoost = 3.0f;

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
    for (size_t i = 0; i < sound.samples.size(); i++) {
        sound.samples[i] *= std::exp(-kDecayPerSecond * static_cast<float>(i) / kCueRate);
    }
    fade_out(sound, kTailSeconds);
    lead_in(sound, kLeadInSeconds);
    return sound;
}

/* The opponent's pulse, lower and rounder than your tick so the two never
 * blend: a 400 Hz sine with a little of its octave, rising over 5 ms and
 * dying away over 70 ms, its last 10 ms faded to silence, after the tick's
 * 12 ms of lead-in. 82 ms in all, 164 ms an octave down. */
Sound pulse_sound() {
    constexpr double kHz = 400.0;
    constexpr float kOctave = 0.25f; /* the octave's share of the fundamental */
    constexpr float kSeconds = 0.07f;
    constexpr float kAttackSeconds = 0.005f;
    constexpr float kDecayPerSecond = 30.0f;
    constexpr float kAmplitude = 0.95f;
    constexpr float kTailSeconds = 0.01f;
    constexpr float kLeadInSeconds = 0.012f;
    Sound sound;
    sound.samples.resize(static_cast<size_t>(kSeconds * kCueRate));
    for (size_t i = 0; i < sound.samples.size(); i++) {
        const double t = static_cast<double>(i) / kCueRate;
        const double phase = 2.0 * std::numbers::pi * kHz * t;
        const double tone = std::sin(phase) + kOctave * std::sin(2.0 * phase);
        sound.samples[i] = static_cast<float>(tone) * kAmplitude / (1.0f + kOctave) *
                           std::exp(-kDecayPerSecond * static_cast<float>(t));
    }
    fade_in(sound, kAttackSeconds);
    fade_out(sound, kTailSeconds);
    lead_in(sound, kLeadInSeconds);
    return sound;
}

/* A position cue's pulsed voice, its sound made to fit the fastest interval
 * played at the lowest pitch the widest down range gives. */
VoiceSpec position_voice(const char* name, int cue, Sound sound, float fastest_interval) {
    VoiceSpec voice;
    voice.name = name;
    voice.cue = cue;
    voice.sound = std::move(sound);
    voice.mode = VoiceMode::pulsed;
    voice.fastest_interval = fastest_interval;
    voice.lowest_pitch = std::exp2(-MatchCues::kWidestDownRange);
    return voice;
}

}  // namespace

StageMapping MatchCues::StageValues::mapping() const {
    return StageMapping{edge_pan.value(), up_range.value(), down_range.value()};
}

YourMapping MatchCues::YourValues::mapping() const {
    return YourMapping{stage.mapping(), slow_interval.value(), fast_interval.value(),
        far_distance.value(), near_distance.value()};
}

OpponentMapping MatchCues::OpponentValues::mapping() const {
    return OpponentMapping{stage.mapping(), interval.value()};
}

/* Worded for either position cue, each registering its own copy. */
MatchCues::StageValues MatchCues::add_stage_values(ValueGroup group) {
    const StageMapping defaults;
    return StageValues{
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
    };
}

MatchCues::YourValues MatchCues::add_your_values(ValueGroup group) {
    const YourMapping defaults;
    StageValues stage = add_stage_values(group);
    return YourValues{
        stage,
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

MatchCues::OpponentValues MatchCues::add_opponent_values(ValueGroup group) {
    const OpponentMapping defaults;
    StageValues stage = add_stage_values(group);
    return OpponentValues{
        stage,
        group.add_number(NumberSpec{"interval", "Interval",
            "Time between the opponent's pulses, the same however near they are.", "s",
            defaults.interval, kOpponentFastestInterval, 3.0f, 0.05f}),
    };
}

MatchCues::MatchCues(CueSettings& settings, CueEngine& engine)
    : m_engine(engine),
      m_your_cue(settings.add_cue("your_position", "Your position cue", kYourBoost)),
      m_your(add_your_values(m_your_cue.group)),
      m_opponent_cue(
          settings.add_cue("opponent_position", "Opponent's position cue", kOpponentBoost)),
      m_opponent(add_opponent_values(m_opponent_cue.group)) {
    m_your_voice = m_engine.add_voice(
        position_voice("your position", m_your_cue.values.cue, tick_sound(), kFastestInterval));
    m_opponent_voice = m_engine.add_voice(position_voice(
        "opponent position", m_opponent_cue.values.cue, pulse_sound(), kOpponentFastestInterval));
}

void MatchCues::play(int voice, const std::optional<VoiceTargets>& targets) {
    if (voice < 0) {
        return;
    }
    if (targets.has_value()) {
        m_engine.start(voice, *targets);
    } else if (m_engine.playing(voice)) {
        m_engine.stop(voice);
    }
}

void MatchCues::tick(const A11yMatchState& match) {
    play(m_your_voice, your_position(match, m_your.mapping()));
    play(m_opponent_voice, opponent_position(match, m_opponent.mapping()));
    if (match.running) {
        m_engine.trace_tick(m_tick++);
    }
}

}  // namespace a11y
