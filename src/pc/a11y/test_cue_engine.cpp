/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cue engine rendered into buffers, with no device. This file supplies
 * the pc_log_line that main.c normally provides. Its one argument is where
 * to write a dump of a known pulse train, which tools/a11y/test_cue_dump.py
 * then reads back through the dump analysis script. */
#include "cue_engine.hpp"
#include "cue_sounds.hpp"
#include "miniaudio.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using a11y::CueEngine;
using a11y::CueEngineOptions;
using a11y::CueLevel;
using a11y::kCueRate;
using a11y::Sound;
using a11y::VoiceMode;
using a11y::VoiceSpec;
using a11y::VoiceTargets;

std::vector<std::string> s_lines;

/* One voice at full scale, centred, at full volume: the mix's headroom. */
const float kCentredFull = 0.5f * std::sqrt(0.5f);

Sound constant(float value, int frames) {
    Sound sound;
    sound.samples.assign(static_cast<size_t>(frames), value);
    return sound;
}

Sound sine(double hz, int frames, double amplitude) {
    ma_waveform_config config =
        ma_waveform_config_init(ma_format_f32, 1, kCueRate, ma_waveform_type_sine, amplitude, hz);
    ma_waveform wave;
    assert(ma_waveform_init(&config, &wave) == MA_SUCCESS);
    Sound sound = a11y::read_sound(&wave, frames);
    ma_waveform_uninit(&wave);
    return sound;
}

VoiceSpec looped(const char* name, Sound sound) {
    VoiceSpec spec;
    spec.name = name;
    spec.sound = std::move(sound);
    spec.mode = VoiceMode::looped;
    return spec;
}

VoiceSpec pulsed(const char* name, Sound sound, float fastest) {
    VoiceSpec spec;
    spec.name = name;
    spec.sound = std::move(sound);
    spec.mode = VoiceMode::pulsed;
    spec.fastest_interval = fastest;
    spec.lowest_pitch = 1.0f;
    return spec;
}

VoiceTargets at(float pan, float interval = 0.5f, float gain = 1.0f) {
    VoiceTargets targets;
    targets.pan = pan;
    targets.gain = gain;
    targets.interval = interval;
    return targets;
}

/* Interleaved stereo, rendered in blocks the size of a WASAPI callback. */
std::vector<float> render(CueEngine& engine, int frames) {
    std::vector<float> out(static_cast<size_t>(frames) * 2);
    for (int done = 0; done < frames; done += 480) {
        engine.render(out.data() + static_cast<size_t>(done) * 2, std::min(480, frames - done));
    }
    return out;
}

/* Mean of left² + right² per frame. */
double power(const std::vector<float>& out) {
    double sum = 0.0;
    for (float s : out) {
        sum += static_cast<double>(s) * s;
    }
    return sum / static_cast<double>(out.size() / 2);
}

/* The largest step between neighbouring samples of one channel, from the
 * sample before the buffer. */
float largest_step(const std::vector<float>& out, int channel, float before = 0.0f) {
    float largest = 0.0f;
    float last = before;
    for (size_t i = static_cast<size_t>(channel); i < out.size(); i += 2) {
        largest = std::max(largest, std::fabs(out[i] - last));
        last = out[i];
    }
    return largest;
}

float peak(const std::vector<float>& out) {
    float largest = 0.0f;
    for (float s : out) {
        largest = std::max(largest, std::fabs(s));
    }
    return largest;
}

/* Frames where sound begins after silence. */
std::vector<int> onsets(const std::vector<float>& out) {
    std::vector<int> found;
    bool silent = true;
    for (size_t f = 0; f < out.size() / 2; f++) {
        bool sounding = std::fabs(out[2 * f]) > 1e-7f || std::fabs(out[2 * f + 1]) > 1e-7f;
        if (sounding && silent) {
            found.push_back(static_cast<int>(f));
        }
        silent = !sounding;
    }
    return found;
}

/* Frames where one channel's sound begins after silence. */
std::vector<int> channel_onsets(const std::vector<float>& out, int channel) {
    std::vector<int> found;
    bool silent = true;
    for (size_t f = 0; f < out.size() / 2; f++) {
        bool sounding = std::fabs(out[2 * f + static_cast<size_t>(channel)]) > 1e-7f;
        if (sounding && silent) {
            found.push_back(static_cast<int>(f));
        }
        silent = !sounding;
    }
    return found;
}

bool near(double value, double expected, double tolerance) {
    return std::fabs(value - expected) <= tolerance;
}

bool logged(const std::string& line) {
    return std::find(s_lines.begin(), s_lines.end(), line) != s_lines.end();
}

void constant_power_pan_keeps_loudness() {
    /* 500 Hz, a whole number of periods, so the loop has no seam. */
    double centre = 0.0;
    for (float pan : {0.0f, -1.0f, -0.5f, 0.5f, 1.0f}) {
        CueEngine engine;
        int voice = engine.add_voice(looped("tone", sine(500.0, 4800, 0.5)));
        engine.start(voice, at(pan));
        render(engine, kCueRate / 10);
        std::vector<float> out = render(engine, kCueRate / 10);
        double p = power(out);
        if (pan == 0.0f) {
            centre = p;
        }
        assert(p > 0.0);
        assert(near(p, centre, centre * 0.01));
        if (pan == -1.0f) {
            assert(largest_step(out, 1) < 1e-6f); /* nothing on the right */
        }
    }
}

void starts_and_stops_do_not_jump() {
    /* A constant sound: any step comes from the voice's gain alone. Cut at
     * full level it would step by kCentredFull at once. */
    const float most = kCentredFull / 50.0f;
    CueEngine engine;
    int voice = engine.add_voice(looped("hum", constant(1.0f, 4800)));
    engine.start(voice, at(0.0f));
    std::vector<float> out = render(engine, kCueRate / 20);
    assert(largest_step(out, 0) < most);
    assert(near(out[out.size() - 2], kCentredFull, 1e-4));

    /* A gain target that jumps glides instead. */
    engine.set_targets(voice, at(0.0f, 0.5f, 0.2f));
    float before = out[out.size() - 2];
    out = render(engine, kCueRate / 10);
    assert(largest_step(out, 0, before) < most);
    assert(near(out[out.size() - 2], 0.2f * kCentredFull, 1e-4));

    /* A pan target that jumps glides too. */
    engine.set_targets(voice, at(1.0f, 0.5f, 0.2f));
    before = out[out.size() - 2];
    out = render(engine, kCueRate / 10);
    assert(largest_step(out, 0, before) < most);

    engine.stop(voice);
    before = out[out.size() - 1];
    out = render(engine, kCueRate / 20);
    assert(largest_step(out, 1, before) < most);
    assert(out[out.size() - 1] == 0.0f);
    assert(!engine.playing(voice));

    /* Started again while still ramping down: it ramps back up from where
     * it was. */
    engine.start(voice, at(0.0f));
    render(engine, kCueRate / 20);
    engine.stop(voice);
    std::vector<float> down = render(engine, 48);
    engine.start(voice, at(0.0f));
    std::vector<float> back = render(engine, kCueRate / 20);
    assert(largest_step(back, 0, down[down.size() - 2]) < most);
    assert(near(back[back.size() - 2], kCentredFull, 1e-4));
}

void check_onsets(const std::vector<int>& found, const std::vector<int>& expected) {
    if (found != expected) {
        for (int f : found) {
            std::cerr << f << " ";
        }
        std::cerr << "\n";
    }
    assert(found == expected);
}

void pulses_follow_their_interval() {
    CueEngine engine;
    int voice = engine.add_voice(pulsed("tick", constant(0.5f, 480), 0.02f));
    engine.start(voice, at(0.0f, 0.1f));
    std::vector<float> out = render(engine, 14500);
    /* An interval change after a pulse applies to the next one... */
    engine.set_targets(voice, at(0.0f, 0.05f));
    std::vector<float> more = render(engine, 2600);
    out.insert(out.end(), more.begin(), more.end());
    /* ...and so does one made between pulses, counted from the last. */
    engine.set_targets(voice, at(0.0f, 0.2f));
    more = render(engine, 15000);
    out.insert(out.end(), more.begin(), more.end());
    /* Held to the voice's fastest interval; one shorter than the time
     * already waited starts the rhythm afresh at once. */
    engine.set_targets(voice, at(0.0f, 0.001f));
    more = render(engine, 1900);
    out.insert(out.end(), more.begin(), more.end());

    const std::vector<int> expected = {0, 4800, 9600, 14400, 16800, 26400, 32100, 33060};
    check_onsets(onsets(out), expected);
}

void alternating_voices_pulse_between_each_other() {
    /* Started together, the later added voice waits half an interval. */
    {
        CueEngine engine;
        int left = engine.add_voice(pulsed("left", constant(0.5f, 480), 0.02f));
        VoiceSpec spec = pulsed("right", constant(0.5f, 480), 0.02f);
        spec.alternates_with = left;
        int right = engine.add_voice(std::move(spec));
        assert(right == 1);
        engine.start(left, at(-1.0f, 0.1f));
        engine.start(right, at(1.0f, 0.1f));
        std::vector<float> out = render(engine, 12000);
        check_onsets(channel_onsets(out, 0), {0, 4800, 9600});
        check_onsets(channel_onsets(out, 1), {2400, 7200});
    }
    /* Started later, either one falls between the other's pulses, wherever
     * a block starts. */
    {
        CueEngine engine;
        int left = engine.add_voice(pulsed("left", constant(0.5f, 480), 0.02f));
        VoiceSpec spec = pulsed("right", constant(0.5f, 480), 0.02f);
        spec.alternates_with = left;
        int right = engine.add_voice(std::move(spec));
        engine.start(right, at(1.0f, 0.1f));
        std::vector<float> out = render(engine, 1000);
        engine.start(left, at(-1.0f, 0.1f));
        std::vector<float> more = render(engine, 9000);
        out.insert(out.end(), more.begin(), more.end());
        check_onsets(channel_onsets(out, 1), {0, 4800, 9600});
        check_onsets(channel_onsets(out, 0), {2400, 7200});
        engine.stop(right);
        more = render(engine, 7000);
        out.insert(out.end(), more.begin(), more.end());
        engine.start(right, at(1.0f, 0.1f));
        more = render(engine, 7000);
        out.insert(out.end(), more.begin(), more.end());
        check_onsets(channel_onsets(out, 1), {0, 4800, 9600, 19200});
    }
    /* Only a pulsed voice added earlier and not yet paired can be
     * alternated with. */
    {
        CueEngine engine;
        VoiceSpec spec = pulsed("alone", constant(0.5f, 480), 0.02f);
        spec.alternates_with = 0;
        assert(engine.add_voice(std::move(spec)) == -1);
        assert(logged("[a11y] cue alone refused: no pulsed voice to alternate with"));
        int first = engine.add_voice(pulsed("first", constant(0.5f, 480), 0.02f));
        spec = pulsed("second", constant(0.5f, 480), 0.02f);
        spec.alternates_with = first;
        assert(engine.add_voice(std::move(spec)) != -1);
        spec = pulsed("third", constant(0.5f, 480), 0.02f);
        spec.alternates_with = first;
        assert(engine.add_voice(std::move(spec)) == -1);
        assert(logged("[a11y] cue third refused: no pulsed voice to alternate with"));
    }
}

void the_volume_chain_multiplies() {
    CueEngine engine;
    int voice = engine.add_voice(looped("hum", constant(1.0f, 4800)));
    VoiceSpec other = looped("other", constant(1.0f, 4800));
    other.cue = 1;
    engine.add_voice(other);
    engine.start(voice, at(0.0f));
    std::vector<float> out = render(engine, kCueRate / 10);
    assert(near(out[out.size() - 2], kCentredFull, 1e-4));

    engine.set_master_volume(0.5f);
    engine.set_mix_level(CueLevel{true, 0.8f, 1.0f});
    engine.set_cue_level(0, CueLevel{true, 0.5f, 2.0f});
    engine.set_cue_level(1, CueLevel{true, 0.1f, 1.0f}); /* another cue's */
    engine.set_targets(voice, at(0.0f, 0.5f, 0.6f));
    out = render(engine, kCueRate / 10);
    assert(near(out[out.size() - 2], 0.5 * 0.8 * 0.5 * 2.0 * 0.6 * kCentredFull, 1e-4));

    engine.set_cue_level(0, CueLevel{false, 0.5f, 2.0f});
    out = render(engine, kCueRate / 10);
    assert(near(out[out.size() - 2], 0.0, 1e-5));
    engine.set_cue_level(0, CueLevel{});
    engine.set_mix_level(CueLevel{false, 1.0f, 1.0f});
    out = render(engine, kCueRate / 10);
    assert(near(out[out.size() - 2], 0.0, 1e-5));
    /* Still playing underneath: on again is heard again. */
    assert(engine.playing(voice));
    engine.set_mix_level(CueLevel{});
    engine.set_master_volume(1.0f);
    out = render(engine, kCueRate / 10);
    assert(near(out[out.size() - 2], 0.6 * kCentredFull, 1e-4));
}

void five_voices_stay_under_full_scale() {
    for (float pan : {0.0f, -1.0f}) {
        CueEngine engine;
        for (int i = 0; i < 5; i++) {
            /* In phase, the worst case. */
            int voice = engine.add_voice(looped("tone", sine(500.0, 4800, 1.0)));
            engine.start(voice, at(pan));
        }
        std::vector<float> out = render(engine, kCueRate / 5);
        assert(peak(out) < 1.0f);
        assert(peak(out) > 0.9f);
    }
}

void a_pulse_that_does_not_fit_is_refused() {
    CueEngine engine;
    s_lines.clear();
    assert(engine.add_voice(pulsed("long", constant(0.5f, 5760), 0.08f)) == -1);
    assert(logged("[a11y] cue long refused: its sound lasts 0.120 s at pitch 1.00, longer than "
                  "its fastest interval of 0.080 s"));
    /* Pitched down, it plays longer. */
    VoiceSpec low = pulsed("low", constant(0.5f, 2400), 0.08f);
    low.lowest_pitch = 0.5f;
    assert(engine.add_voice(low) == -1);
    assert(engine.add_voice(pulsed("fits", constant(0.5f, 2400), 0.08f)) == 0);
    /* A looped sound has no interval to fit. */
    assert(engine.add_voice(looped("hum", constant(0.5f, 96000))) == 1);
}

void starts_and_stops_are_logged_once() {
    CueEngine engine;
    int voice = engine.add_voice(pulsed("tick", constant(0.5f, 480), 0.02f));
    s_lines.clear();
    engine.start(voice, at(0.5f, 0.25f));
    engine.start(voice, at(0.25f, 0.25f));
    engine.stop(voice);
    engine.stop(voice);
    assert(s_lines.size() == 2);
    assert(s_lines[0] == "[a11y] cue tick start pan 0.50 pitch 1.00 gain 1.00 interval 0.250");
    assert(s_lines[1] == "[a11y] cue tick stop pan 0.25 pitch 1.00 gain 1.00 interval 0.250");
}

void the_trace_has_a_line_per_voice_each_tick(const std::string& path) {
    {
        CueEngineOptions options;
        options.trace = path;
        CueEngine engine(options);
        int tick = engine.add_voice(pulsed("tick", constant(0.5f, 480), 0.02f));
        engine.add_voice(looped("ledge right", constant(0.5f, 480)));
        engine.start(tick, at(-0.5f, 0.3f, 0.75f));
        engine.trace_tick(7);
    }
    std::ifstream file(path);
    std::string header, first, second, end;
    std::getline(file, header);
    std::getline(file, first);
    std::getline(file, second);
    assert(header == "# tick\tvoice\tstate\tpan\tpitch\tgain\tinterval");
    assert(first == "7\ttick\ton\t-0.500\t1.000\t0.750\t0.300");
    assert(second == "7\tledge right\toff\t0.000\t1.000\t1.000\t-");
    assert(!std::getline(file, end));
}

/* Two seconds: a 1 kHz tick every 0.25 s on the right, then every 0.125 s
 * on the left, each after 12 ms of lead-in silence. */
void write_pulse_train(const std::string& path) {
    {
        CueEngineOptions options;
        options.dump = path;
        CueEngine engine(options);
        Sound tick = sine(1000.0, kCueRate * 40 / 1000, 0.8);
        a11y::shape_edges(tick, 0.002f);
        a11y::lead_in(tick, 0.012f);
        int voice = engine.add_voice(pulsed("tick", tick, 0.1f));
        engine.start(voice, at(0.5f, 0.25f));
        render(engine, kCueRate);
        engine.set_targets(voice, at(-0.5f, 0.125f));
        render(engine, kCueRate);
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    assert(file.tellg() == static_cast<std::streamoff>(2 * kCueRate * 2 * sizeof(float)));
}

}  // namespace

extern "C" void pc_log_line(const char* fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    s_lines.emplace_back(line);
}

int main(int argc, char** argv) {
    const std::string dump = argc > 1 ? argv[1] : "cue_pulse_train.f32";
    constant_power_pan_keeps_loudness();
    starts_and_stops_do_not_jump();
    pulses_follow_their_interval();
    alternating_voices_pulse_between_each_other();
    the_volume_chain_multiplies();
    five_voices_stay_under_full_scale();
    a_pulse_that_does_not_fit_is_refused();
    starts_and_stops_are_logged_once();
    the_trace_has_a_line_per_voice_each_tick(dump + ".trace");
    write_pulse_train(dump);
    std::cout << "cue_engine: all tests passed\n";
    return 0;
}
