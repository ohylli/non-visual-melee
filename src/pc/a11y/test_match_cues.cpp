/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cues of a match against hand-written snapshots, their voices rendered
 * into a buffer with no device: both position cues register their values in
 * the file's order and their voices fit, and the opponent's cue follows a
 * change of opponent without restarting, goes quiet while the opponent is
 * knocked out and comes back at respawn; the gap warning registers after
 * them, starts over a gap, holds and stops, and its tone loops without a
 * seam. This file supplies the
 * pc_log_line that main.c normally provides. */
#include "cue_engine.hpp"
#include "cue_settings.hpp"
#include "match_cues.hpp"
#include "test_match_snapshot.hpp"
#include "value_model.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace match_test;

std::vector<std::string> s_lines;

std::size_t count_logged_starting(const std::string& start) {
    return static_cast<std::size_t>(std::count_if(s_lines.begin(), s_lines.end(),
        [&](const std::string& line) { return line.rfind(start, 0) == 0; }));
}

/* You at the centre, the opponent in slot `slot` at (x, 0). */
A11yMatchState match_with(int slot, float x) {
    A11yMatchState match{};
    match.running = true;
    match.you = fighter(0, 0.0f, 0.0f);
    match.opponent_count = 1;
    match.opponents[0] = fighter(slot, x, 0.0f);
    match.stage = final_destination();
    return match;
}

/* The engine with logging, the cue settings at their defaults and the
 * match cues. Each test starts with an empty log. */
struct Fixture {
    a11y::CueEngine engine{a11y::CueEngineOptions{true, "", ""}};
    a11y::ValueModel model;
    a11y::CueSettings cues{model};
    a11y::MatchCues match_cues{cues, engine};

    Fixture() {
        cues.apply(engine);
        s_lines.clear();
    }

    /* Only the opponent's cue heard, yours turned off underneath. */
    void opponent_alone() {
        model.set(*model.find("your_position.on"), 0.0f);
        cues.apply(engine);
    }
};

/* Interleaved stereo, rendered in blocks the size of a WASAPI callback. */
std::vector<float> render(a11y::CueEngine& engine, float seconds) {
    const int frames = static_cast<int>(seconds * a11y::kCueRate);
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int done = 0; done < frames; done += 480) {
        engine.render(
            out.data() + static_cast<std::size_t>(done) * 2, std::min(480, frames - done));
    }
    return out;
}

/* Seconds into the buffer where sound starts after at least 30 ms of
 * silence. */
std::vector<double> onsets(const std::vector<float>& stereo) {
    constexpr float kThreshold = 0.01f;
    const std::size_t quiet_frames = a11y::kCueRate * 3 / 100;
    std::vector<double> found;
    std::size_t quiet = quiet_frames;
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        float level = std::max(std::fabs(stereo[frame * 2]), std::fabs(stereo[frame * 2 + 1]));
        if (level < kThreshold) {
            ++quiet;
            continue;
        }
        if (quiet >= quiet_frames) {
            found.push_back(static_cast<double>(frame) / a11y::kCueRate);
        }
        quiet = 0;
    }
    return found;
}

/* Left and right energy from a second onwards. */
void energy(const std::vector<float>& stereo, double from, double& left, double& right) {
    left = 0.0;
    right = 0.0;
    for (std::size_t frame = static_cast<std::size_t>(from * a11y::kCueRate);
        frame < stereo.size() / 2; ++frame)
    {
        left += static_cast<double>(stereo[frame * 2]) * stereo[frame * 2];
        right += static_cast<double>(stereo[frame * 2 + 1]) * stereo[frame * 2 + 1];
    }
}

void both_cues_register_and_fit() {
    Fixture f;
    assert(f.match_cues.your_voice() >= 0 && f.match_cues.opponent_voice() >= 0);
    const auto& groups = f.model.groups();
    assert(groups.size() == 4);
    assert(groups[1].id == "your_position" && groups[2].id == "opponent_position");
    assert(groups[2].title == "Opponent's position cue");
    std::vector<std::string> keys;
    for (const a11y::NamedValue* named : groups[2].values) {
        keys.push_back(named->key);
    }
    assert((keys == std::vector<std::string>{"opponent_position.on", "opponent_position.volume",
                        "opponent_position.boost", "opponent_position.edge_pan",
                        "opponent_position.up_range", "opponent_position.down_range",
                        "opponent_position.interval"}));
    /* Your cue's mapping defaults, each a value of its own. */
    for (const char* name : {"edge_pan", "up_range", "down_range"}) {
        const a11y::NamedValue* yours = f.model.find(std::string("your_position.") + name);
        const a11y::NamedValue* theirs = f.model.find(std::string("opponent_position.") + name);
        assert(yours != nullptr && theirs != nullptr && yours != theirs);
        assert(yours->default_value == theirs->default_value);
        assert(yours->min == theirs->min && yours->max == theirs->max);
    }
    const a11y::NamedValue* interval = f.model.find("opponent_position.interval");
    assert(interval->default_value == 0.4f && interval->unit == "s");
    assert(interval->min == a11y::MatchCues::kOpponentFastestInterval);
    assert(f.model.find("opponent_position.boost")->default_value == 3.0f);
    assert(f.model.find("opponent_position.on")->description ==
           "Turns the opponent's position cue on or off.");
}

void the_gap_warning_registers_after_the_position_cues() {
    Fixture f;
    assert(f.match_cues.gap_voice() >= 0);
    const auto& groups = f.model.groups();
    assert(groups[3].id == "gap_warning" && groups[3].title == "Gap warning");
    std::vector<std::string> keys;
    for (const a11y::NamedValue* named : groups[3].values) {
        keys.push_back(named->key);
    }
    assert((keys == std::vector<std::string>{"gap_warning.on", "gap_warning.volume",
                        "gap_warning.boost", "gap_warning.hold"}));
    const a11y::NamedValue* hold = f.model.find("gap_warning.hold");
    assert(hold->default_value == 10.0f && hold->unit == "ticks");
    assert(f.model.find("gap_warning.boost")->default_value == 1.0f);
    assert(f.model.find("gap_warning.on")->description == "Turns the gap warning on or off.");
}

/* You falling past Final Destination's left edge, nothing below. */
A11yMatchState over_the_gap() {
    A11yMatchState match = match_with(1, 60.0f);
    match.you = fighter(0, -100.0f, -20.0f);
    match.you.airborne = true;
    match.below.line = -1;
    return match;
}

void the_gap_warning_starts_holds_and_stops() {
    Fixture f;
    const int voice = f.match_cues.gap_voice();
    f.model.set(*f.model.find("gap_warning.hold"), 2.0f);
    f.match_cues.tick(over_the_gap());
    assert(f.engine.playing(voice));
    assert(count_logged_starting("[a11y] cue gap start pan 0.00 pitch 1.00 gain 1.00") == 1);
    /* Landed back on the floor: two ticks of hold, then the stop. */
    A11yMatchState landed = match_with(1, 60.0f);
    landed.below = A11yFloorBelow{true, 1, 0.0f};
    f.match_cues.tick(landed);
    f.match_cues.tick(landed);
    assert(f.engine.playing(voice));
    f.match_cues.tick(landed);
    assert(!f.engine.playing(voice));
    assert(count_logged_starting("[a11y] cue gap stop") == 1);
}

void the_gap_tone_loops_without_a_seam() {
    Fixture f;
    f.model.set(*f.model.find("your_position.on"), 0.0f);
    f.model.set(*f.model.find("opponent_position.on"), 0.0f);
    f.cues.apply(f.engine);
    f.match_cues.tick(over_the_gap());
    /* Past the start ramp, across many loops of the buffer: centred, with
     * no step bigger than the tone's own steepest. */
    std::vector<float> out = render(f.engine, 0.6f);
    double left = 0.0;
    double right = 0.0;
    energy(out, 0.1, left, right);
    assert(left > 0.0 && std::fabs(left - right) < left * 1e-3);
    float steepest = 0.0f;
    float loudest = 0.0f;
    for (std::size_t frame = static_cast<std::size_t>(0.1 * a11y::kCueRate) + 1;
        frame < out.size() / 2; ++frame)
    {
        steepest = std::max(steepest, std::fabs(out[frame * 2] - out[(frame - 1) * 2]));
        loudest = std::max(loudest, std::fabs(out[frame * 2]));
    }
    /* At its steepest the tone moves about 0.15 of its peak in a sample; a
     * seam would jump by far more. */
    assert(loudest > 0.05f);
    assert(steepest < loudest * 0.2f);
}

void a_change_of_opponent_moves_the_voice_without_restarting_it() {
    Fixture f;
    f.opponent_alone();
    f.match_cues.tick(match_with(1, -60.0f));
    std::vector<float> first = render(f.engine, 0.15f);
    /* Another fighter comes nearer, on the other side. */
    f.match_cues.tick(match_with(2, 60.0f));
    assert(f.engine.playing(f.match_cues.opponent_voice()));
    std::vector<float> second = render(f.engine, 0.6f);
    assert(count_logged_starting("[a11y] cue opponent position start") == 1);
    assert(count_logged_starting("[a11y] cue opponent position stop") == 0);
    std::vector<float> all = first;
    all.insert(all.end(), second.begin(), second.end());
    /* One pulse at the start and the next a steady interval later, none at
     * the change. */
    std::vector<double> at = onsets(all);
    assert(at.size() == 2);
    assert(at[0] < 0.02);
    assert(std::fabs(at[1] - at[0] - 0.4) < 0.002);
    double left = 0.0;
    double right = 0.0;
    energy(first, 0.0, left, right);
    assert(left > right * 2.0);
    energy(all, 0.4, left, right);
    assert(right > left * 2.0);
}

void the_opponent_cue_stops_while_knocked_out_and_comes_back() {
    Fixture f;
    const int voice = f.match_cues.opponent_voice();
    A11yMatchState match = match_with(1, 30.0f);
    f.match_cues.tick(match);
    assert(f.engine.playing(voice));
    /* Knocked out: the snapshot keeps it, or drops it when nobody else is
     * in play. */
    A11yMatchState out = match;
    out.opponents[0].out = true;
    f.match_cues.tick(out);
    assert(!f.engine.playing(voice));
    A11yMatchState nobody = match;
    nobody.opponent_count = 0;
    f.match_cues.tick(nobody);
    assert(!f.engine.playing(voice));
    /* Your cue goes on meanwhile. */
    assert(f.engine.playing(f.match_cues.your_voice()));
    /* Back on the respawn platform, high above the middle. */
    A11yMatchState respawned = match;
    respawned.opponents[0].x = 0.0f;
    respawned.opponents[0].y = 60.0f;
    f.match_cues.tick(respawned);
    assert(f.engine.playing(voice));
    assert(count_logged_starting("[a11y] cue opponent position start") == 2);
    assert(count_logged_starting("[a11y] cue opponent position stop") == 1);
    assert(count_logged_starting("[a11y] cue your position start") == 1);
}

}  // namespace

extern "C" void pc_log_line(const char* fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    s_lines.emplace_back(line);
}

int main() {
    both_cues_register_and_fit();
    the_gap_warning_registers_after_the_position_cues();
    the_gap_warning_starts_holds_and_stops();
    the_gap_tone_loops_without_a_seam();
    a_change_of_opponent_moves_the_voice_without_restarting_it();
    the_opponent_cue_stops_while_knocked_out_and_comes_back();
    std::cout << "match_cues: all tests passed\n";
    return 0;
}
