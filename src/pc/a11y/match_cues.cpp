/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "match_cues.hpp"
#include "cue_sounds.hpp"
#include "miniaudio.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace a11y {
namespace {

/* The cues' ids, as add_cue takes them and the menus' Preview rows hold
 * them. */
constexpr std::string_view kYourId = "your_position";
constexpr std::string_view kOpponentId = "opponent_position";
constexpr std::string_view kGapId = "gap_warning";
constexpr std::string_view kLedgeId = "ledge_warning";

/* A short click lifted under its volume to sit level with held tones. */
constexpr float kYourBoost = 2.0f;
/* The opponent's lower pulse more, as tuned by ear. */
constexpr float kOpponentBoost = 3.0f;
/* A held tone needs no lift. */
constexpr float kGapBoost = 1.0f;
/* The ledge warning's chord, as tuned by ear. */
constexpr float kLedgeBoost = 2.0f;

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

/* The gap warning's tone, held and below both position cues: 120 Hz with
 * its third and fifth harmonics at a third and a fifth of its level, the
 * start of a square wave, a hollow hum that is no pulse and no sine. Its
 * fundamental sits under the opponent's lowest note, 200 Hz at the bottom
 * blast zone, and the harmonics give small speakers something to play.
 * 120 Hz is 400 samples a period, so a buffer of whole periods loops
 * without a seam. Scaled by the amplitudes' sum, it peaks at 0.6 of full
 * scale, 0.5 as RMS, which in the mix sits a little under the opponent's
 * pulse, measured over a tenth of a second, and above your tick: a low
 * tone sounds quieter than a higher one at the same level, and the warning
 * should be heard. */
Sound hum_sound() {
    constexpr int kPeriod = kCueRate / 120;
    constexpr int kPeriods = 12;
    constexpr float kThird = 1.0f / 3.0f;
    constexpr float kFifth = 1.0f / 5.0f;
    Sound sound;
    sound.samples.resize(static_cast<size_t>(kPeriod * kPeriods));
    for (size_t i = 0; i < sound.samples.size(); i++) {
        const double phase = 2.0 * std::numbers::pi * static_cast<double>(i % kPeriod) / kPeriod;
        const double tone =
            std::sin(phase) + kThird * std::sin(3.0 * phase) + kFifth * std::sin(5.0 * phase);
        sound.samples[i] = static_cast<float>(tone) / (1.0f + kThird + kFifth);
    }
    return sound;
}

/* The ledge warning's pulse, a chord unlike the other cues' single tones:
 * 660 and 880 Hz, a just perfect fourth (3:4) as in the Star Fox 64 fork's
 * beside chord, between the opponent's 400 Hz pulse and your 1500 Hz tick
 * on the floor, where ledges are. 90 ms rising over 5 ms and easing off,
 * its last 20 ms faded to silence, after the tick's 12 ms of lead-in: 102
 * ms in all, half of each 0.2 s pulse, so the pulses stay apart. Played
 * only at its own pitch. */
Sound chord_sound() {
    constexpr double kLow = 660.0;
    constexpr double kHigh = 880.0;
    constexpr float kSeconds = 0.09f;
    constexpr float kAttackSeconds = 0.005f;
    constexpr float kDecayPerSecond = 8.0f;
    constexpr float kAmplitude = 0.95f;
    constexpr float kTailSeconds = 0.02f;
    constexpr float kLeadInSeconds = 0.012f;
    Sound sound;
    sound.samples.resize(static_cast<size_t>(kSeconds * kCueRate));
    for (size_t i = 0; i < sound.samples.size(); i++) {
        const double t = static_cast<double>(i) / kCueRate;
        const double tone = std::sin(2.0 * std::numbers::pi * kLow * t) +
                            std::sin(2.0 * std::numbers::pi * kHigh * t);
        sound.samples[i] = static_cast<float>(tone) * kAmplitude / 2.0f *
                           std::exp(-kDecayPerSecond * static_cast<float>(t));
    }
    fade_in(sound, kAttackSeconds);
    fade_out(sound, kTailSeconds);
    lead_in(sound, kLeadInSeconds);
    return sound;
}

/* A ledge warning voice, one a side. On a small floor both sides sound at
 * once with the same chord at the same rate; the right one alternates with
 * the left, so the two are heard as two places rather than one sound in
 * the middle. */
VoiceSpec ledge_voice(const char* name, int cue, int alternates_with = -1) {
    VoiceSpec voice;
    voice.name = name;
    voice.cue = cue;
    voice.sound = chord_sound();
    voice.mode = VoiceMode::pulsed;
    voice.fastest_interval = kLedgeInterval;
    voice.lowest_pitch = 1.0f;
    voice.alternates_with = alternates_with;
    return voice;
}

/* A position cue's pulsed voice, its sound made to fit the fastest interval
 * played at the lowest pitch the widest pitch range gives. */
VoiceSpec position_voice(const char* name, int cue, Sound sound, float fastest_interval) {
    VoiceSpec voice;
    voice.name = name;
    voice.cue = cue;
    voice.sound = std::move(sound);
    voice.mode = VoiceMode::pulsed;
    voice.fastest_interval = fastest_interval;
    voice.lowest_pitch = std::exp2(-MatchCues::kWidestPitchRange / 2.0f);
    return voice;
}

/* A voice's twin for the preview: the same sound and limits, named apart
 * in the log, sounding past the switches, and alone: the preview's two
 * ledge voices never sound together. */
VoiceSpec preview_of(VoiceSpec spec) {
    spec.name += " preview";
    spec.ignores_switches = true;
    spec.alternates_with = -1;
    return spec;
}

}  // namespace

StageMapping MatchCues::StageValues::mapping(bool pan_to_camera) const {
    return StageMapping{pan_to_camera, pitch_range.value()};
}

YourMapping MatchCues::YourValues::mapping(bool pan_to_camera) const {
    return YourMapping{stage.mapping(pan_to_camera), slow_interval.value(), fast_interval.value(),
        far_distance.value(), near_distance.value()};
}

OpponentMapping MatchCues::OpponentValues::mapping(bool pan_to_camera) const {
    return OpponentMapping{stage.mapping(pan_to_camera), interval.value()};
}

GapMapping MatchCues::GapValues::mapping() const {
    return GapMapping{static_cast<int>(std::lround(hold.value()))};
}

LedgeMapping MatchCues::LedgeValues::mapping() const {
    return LedgeMapping{distance.value(), pan_floor.value(), level_floor.value() / 100.0f};
}

/* Worded for either position cue, each registering its own copy. */
MatchCues::StageValues MatchCues::add_stage_values(ValueGroup group) {
    const StageMapping defaults;
    return StageValues{
        group.add_number(NumberSpec{"pitch_range", "Pitch range",
            "How many octaves the pitch rises from the bottom blast zone to the top of the "
            "camera's view; midway it is the cue's own pitch.",
            "", defaults.pitch_range, 0.0f, kWidestPitchRange, 0.1f}),
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

MatchCues::GapValues MatchCues::add_gap_values(ValueGroup group) {
    const GapMapping defaults;
    return GapValues{
        group.add_number(NumberSpec{"hold", "Hold",
            "How long the warning sounds on after you leave a gap, so it does not flicker at a "
            "floor's edge. A tick is a sixtieth of a second.",
            "ticks", static_cast<float>(defaults.hold_ticks), 0.0f, 60.0f, 1.0f}),
    };
}

MatchCues::LedgeValues MatchCues::add_ledge_values(ValueGroup group) {
    const LedgeMapping defaults;
    return LedgeValues{
        group.add_number(NumberSpec{"distance", "Ledge warning distance",
            "How far from the edge the warning starts, in stage units. Final Destination is "
            "about 170 wide.",
            "units", defaults.distance, 5.0f, 100.0f, 1.0f, true}),
        group.add_number(NumberSpec{"pan_floor", "Pan floor",
            "How far to the side the warning sounds at the edge itself; at the warning distance "
            "it is fully to that side.",
            "", defaults.pan_floor, 0.0f, 1.0f, 0.05f}),
        group.add_number(NumberSpec{"level_floor", "Level floor",
            "How loud the warning starts at the warning distance; at the edge it is full.", "%",
            std::round(defaults.level_floor * 100.0f), 0.0f, 100.0f, 5.0f}),
        group.add_number(NumberSpec{"edge_offset", "Edge offset",
            "How far past an end of the floor below you the test for a floor further down looks; "
            "with none there, the end is deadly. Battlefield's side platforms end about 11 units "
            "inside its main floor, so more than 10 would take their ends for deadly.",
            "units", kEdgeOffset, 2.0f, 10.0f, 0.5f}),
    };
}

MatchCues::MatchCues(CueSettings& settings, CueEngine& engine)
    : m_engine(engine),
      m_your_cue(settings.add_cue(std::string(kYourId), "Your position cue", kYourBoost)),
      m_your(add_your_values(m_your_cue.group)),
      m_opponent_cue(
          settings.add_cue(std::string(kOpponentId), "Opponent's position cue", kOpponentBoost)),
      m_opponent(add_opponent_values(m_opponent_cue.group)),
      m_pan_to_camera(settings.add_shared("position_cues", "Position cues")
              .add_switch(SwitchSpec{"pan_to_camera", "Position cues follow the camera",
                  "On, the cues pan across what the camera shows; off, across the whole stage to "
                  "the side blast zones.",
                  StageMapping{}.pan_to_camera, true})),
      m_gap_cue(settings.add_cue(std::string(kGapId), "Gap warning", kGapBoost)),
      m_gap(add_gap_values(m_gap_cue.group)),
      m_ledge_cue(settings.add_cue(std::string(kLedgeId), "Ledge warning", kLedgeBoost)),
      m_ledge(add_ledge_values(m_ledge_cue.group)) {
    VoiceSpec your =
        position_voice("your position", m_your_cue.values.cue, tick_sound(), kFastestInterval);
    VoiceSpec opponent = position_voice(
        "opponent position", m_opponent_cue.values.cue, pulse_sound(), kOpponentFastestInterval);
    VoiceSpec gap;
    gap.name = "gap";
    gap.cue = m_gap_cue.values.cue;
    gap.sound = hum_sound();
    gap.mode = VoiceMode::looped;
    VoiceSpec ledge_left = ledge_voice("ledge left", m_ledge_cue.values.cue);
    m_your_voice = m_engine.add_voice(your);
    m_opponent_voice = m_engine.add_voice(opponent);
    m_gap_voice = m_engine.add_voice(gap);
    m_ledge_left_voice = m_engine.add_voice(ledge_left);
    VoiceSpec ledge_right = ledge_voice("ledge right", m_ledge_cue.values.cue, m_ledge_left_voice);
    m_ledge_right_voice = m_engine.add_voice(ledge_right);
    m_preview_voices.your = m_engine.add_voice(preview_of(std::move(your)));
    m_preview_voices.opponent = m_engine.add_voice(preview_of(std::move(opponent)));
    m_preview_voices.gap = m_engine.add_voice(preview_of(std::move(gap)));
    m_preview_voices.ledge_left = m_engine.add_voice(preview_of(std::move(ledge_left)));
    m_preview_voices.ledge_right = m_engine.add_voice(preview_of(std::move(ledge_right)));
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

void MatchCues::silence() {
    /* The match's voices alone: a preview plays on over a silent match. */
    for (int voice :
        {m_your_voice, m_opponent_voice, m_gap_voice, m_ledge_left_voice, m_ledge_right_voice})
    {
        play(voice, std::nullopt);
    }
    m_gap_warning.forget();
}

MatchCues::Preview MatchCues::preview_named(std::string_view id) {
    if (id == kYourId) {
        return Preview::your;
    }
    if (id == kOpponentId) {
        return Preview::opponent;
    }
    if (id == kGapId) {
        return Preview::gap;
    }
    if (id == kLedgeId) {
        return Preview::ledge;
    }
    return Preview::none;
}

bool MatchCues::preview(std::string_view id, Clock::time_point now) {
    const Preview which = preview_named(id);
    if (which == Preview::none) {
        return false;
    }
    stop_preview(now);
    m_preview = which;
    m_preview_start = std::max(now, m_preview_quiet);
    preview_frame(now);
    return true;
}

void MatchCues::stop_preview(Clock::time_point now) {
    const PreviewVoices& v = m_preview_voices;
    bool stopped = false;
    for (int voice : {v.your, v.opponent, v.gap, v.ledge_left, v.ledge_right}) {
        if (voice >= 0 && m_engine.playing(voice)) {
            m_engine.stop(voice);
            stopped = true;
        }
    }
    if (stopped) {
        m_preview_quiet = now + kPreviewRestart;
    }
    m_preview = Preview::none;
}

void MatchCues::preview_frame(Clock::time_point now) {
    if (m_preview == Preview::none || now < m_preview_start) {
        return;
    }
    const double seconds = std::chrono::duration<double>(now - m_preview_start).count();
    const PreviewVoices& v = m_preview_voices;
    /* The fighter sits at the centre, where the pan's frame changes
     * nothing. */
    const bool pan_to_camera = m_pan_to_camera.on();
    std::optional<VoiceTargets> targets;
    switch (m_preview) {
    case Preview::your:
        targets = your_position_preview(seconds, m_your.mapping(pan_to_camera));
        if (targets.has_value()) {
            play(v.your, targets);
            return;
        }
        break;
    case Preview::opponent:
        targets = opponent_position_preview(seconds, m_opponent.mapping(pan_to_camera));
        if (targets.has_value()) {
            play(v.opponent, targets);
            return;
        }
        break;
    case Preview::gap:
        targets = gap_warning_preview(seconds);
        if (targets.has_value()) {
            play(v.gap, targets);
            return;
        }
        break;
    case Preview::ledge:
        if (std::optional<LedgeTargets> ledge = ledge_warning_preview(seconds, m_ledge.mapping())) {
            play(v.ledge_left, ledge->left);
            play(v.ledge_right, ledge->right);
            return;
        }
        break;
    case Preview::none:
        return;
    }
    /* Its end: the voices stop, and wait before they may start again. */
    stop_preview(now);
}

void MatchCues::tick(const A11yMatchState& match) {
    const bool pan_to_camera = m_pan_to_camera.on();
    play(m_your_voice, your_position(match, m_your.mapping(pan_to_camera)));
    play(m_opponent_voice, opponent_position(match, m_opponent.mapping(pan_to_camera)));
    play(m_gap_voice, m_gap_warning.tick(match, m_gap.mapping()));
    const LedgeTargets ledge = ledge_warning(match, m_ledge.mapping());
    play(m_ledge_left_voice, ledge.left);
    play(m_ledge_right_voice, ledge.right);
    if (match.running) {
        m_engine.trace_tick(m_tick++);
    }
}

}  // namespace a11y
