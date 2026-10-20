/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cues of a match against hand-written snapshots, their voices rendered
 * into a buffer with no device: both position cues register their values in
 * the file's order and their voices fit, the opponent's cue follows a
 * change of opponent without restarting, goes quiet while the opponent is
 * knocked out and comes back at respawn, and one switch frames both pans;
 * the gap warning registers after them, starts over a gap, holds and
 * stops, and its tone loops without a seam; the ledge warning registers
 * last, with the snapshot's edge offset, starts on each side near a deadly
 * end and stops away from it or over a gap, and pulses 5 times a second on
 * its side; silenced for the port menu, every voice stops and the next tick
 * starts the cues afresh. This file supplies the pc_log_line that main.c
 * normally provides. */
#include "cue_engine.hpp"
#include "cue_settings.hpp"
#include "match_cues.hpp"
#include "speech.hpp"
#include "switches.hpp"
#include "test_match_snapshot.hpp"
#include "value_model.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <numbers>
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
    assert(groups.size() == 6);
    assert(groups[1].id == "your_position" && groups[2].id == "opponent_position");
    assert(groups[2].title == "Opponent's position cue");
    /* After both, the value they share, one switch for both pans. */
    assert(groups[3].id == "position_cues" && groups[3].title == "Position cues");
    assert(groups[3].values.size() == 1);
    const a11y::NamedValue* pan_to_camera = f.model.find("position_cues.pan_to_camera");
    assert(pan_to_camera != nullptr && pan_to_camera->kind == a11y::ValueKind::toggle);
    assert(pan_to_camera->default_value == 1.0f);
    std::vector<std::string> keys;
    for (const a11y::NamedValue* named : groups[2].values) {
        keys.push_back(named->key);
    }
    assert((keys == std::vector<std::string>{"opponent_position.on", "opponent_position.volume",
                        "opponent_position.boost", "opponent_position.pitch_range",
                        "opponent_position.interval"}));
    /* Your cue's pitch range default, a value of its own. */
    const a11y::NamedValue* yours = f.model.find("your_position.pitch_range");
    const a11y::NamedValue* theirs = f.model.find("opponent_position.pitch_range");
    assert(yours != nullptr && theirs != nullptr && yours != theirs);
    assert(yours->default_value == theirs->default_value);
    assert(yours->min == theirs->min && yours->max == theirs->max);
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
    assert(groups[4].id == "gap_warning" && groups[4].title == "Gap warning");
    std::vector<std::string> keys;
    for (const a11y::NamedValue* named : groups[4].values) {
        keys.push_back(named->key);
    }
    assert((keys == std::vector<std::string>{"gap_warning.on", "gap_warning.volume",
                        "gap_warning.boost", "gap_warning.hold"}));
    const a11y::NamedValue* hold = f.model.find("gap_warning.hold");
    assert(hold->default_value == 10.0f && hold->unit == "ticks");
    assert(f.model.find("gap_warning.boost")->default_value == 1.0f);
    assert(f.model.find("gap_warning.on")->description == "Turns the gap warning on or off.");
}

/* A screen reader that hears nothing, for the switches' speech. */
class NoBridge final : public a11y::ScreenReaderBridge {
public:
    std::string init() override { return ""; }
    void shutdown() override {}
    std::string output(std::string_view, bool) override { return ""; }
};

/* The menus' rows, as the spec's Rows table has them, registered as the
 * game registers them: the switches first, then the cues. */
void the_menus_rows_are_the_specs_rows() {
    a11y::Speech speech{a11y::Config{false, false}, std::make_unique<NoBridge>()};
    a11y::CueEngine engine{a11y::CueEngineOptions{false, "", ""}};
    a11y::ValueModel model;
    a11y::Switches switches{model, speech, false};
    a11y::CueSettings cues{model};
    a11y::MatchCues match_cues{cues, engine};
    std::vector<std::string> rows;
    for (const a11y::ShownRow& row : model.shown()) {
        rows.push_back(row.value != nullptr ?
                           row.value->key + " " + row.value->label :
                           "preview " + row.action->group + " " + row.action->label);
    }
    assert((rows == std::vector<std::string>{
                        "accessibility.on Accessibility",
                        "speech.on Speech",
                        "steering.on Steering",
                        "cues.on Cues",
                        "cues.volume Cue volume",
                        "your_position.on Your position cue",
                        "your_position.volume Your position cue volume",
                        "preview your_position Preview your position cue",
                        "opponent_position.on Opponent's position cue",
                        "opponent_position.volume Opponent's position cue volume",
                        "preview opponent_position Preview opponent's position cue",
                        "position_cues.pan_to_camera Position cues follow the camera",
                        "gap_warning.on Gap warning",
                        "gap_warning.volume Gap warning volume",
                        "preview gap_warning Preview gap warning",
                        "ledge_warning.on Ledge warning",
                        "ledge_warning.volume Ledge warning volume",
                        "preview ledge_warning Preview ledge warning",
                        "ledge_warning.distance Ledge warning distance",
                    }));
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

void the_ledge_warning_registers_last() {
    Fixture f;
    assert(f.match_cues.ledge_left_voice() >= 0 && f.match_cues.ledge_right_voice() >= 0);
    const auto& groups = f.model.groups();
    assert(groups[5].id == "ledge_warning" && groups[5].title == "Ledge warning");
    std::vector<std::string> keys;
    for (const a11y::NamedValue* named : groups[5].values) {
        keys.push_back(named->key);
    }
    assert((keys == std::vector<std::string>{"ledge_warning.on", "ledge_warning.volume",
                        "ledge_warning.boost", "ledge_warning.distance", "ledge_warning.pan_floor",
                        "ledge_warning.level_floor", "ledge_warning.edge_offset"}));
    const a11y::NamedValue* distance = f.model.find("ledge_warning.distance");
    assert(distance->default_value == 40.0f && distance->unit == "units");
    assert(f.model.find("ledge_warning.pan_floor")->default_value == 0.15f);
    const a11y::NamedValue* level = f.model.find("ledge_warning.level_floor");
    assert(level->default_value == 50.0f && level->unit == "%");
    /* The snapshot's deadly test reads the offset from the settings. */
    const a11y::NamedValue* offset = f.model.find("ledge_warning.edge_offset");
    assert(offset->default_value == 5.0f && offset->unit == "units");
    assert(f.match_cues.edge_offset() == 5.0f);
    f.model.set(*offset, 8.0f);
    assert(f.match_cues.edge_offset() == 8.0f);
}

/* You on Final Destination's main floor at x, both its ends deadly, the
 * opponent far off on the right. */
A11yMatchState near_the_edges(float x, bool airborne) {
    A11yMatchState match = match_with(1, 60.0f);
    match.you = fighter(0, x, 0.0f);
    match.you.airborne = airborne;
    match.below = A11yFloorBelow{true, 1, 0.0f, {-85.6f, 0.0f, true}, {85.6f, 0.0f, true}};
    return match;
}

void the_ledge_warning_starts_on_each_side_and_stops() {
    Fixture f;
    const int left = f.match_cues.ledge_left_voice();
    const int right = f.match_cues.ledge_right_voice();
    /* 20 units from the left end: halfway in. */
    f.match_cues.tick(near_the_edges(-65.6f, false));
    assert(f.engine.playing(left) && !f.engine.playing(right));
    assert(count_logged_starting(
               "[a11y] cue ledge left start pan -0.58 pitch 1.00 gain 0.75 interval 0.200") == 1);
    /* At the edge: only its targets change. */
    f.match_cues.tick(near_the_edges(-85.6f, false));
    assert(count_logged_starting("[a11y] cue ledge left start") == 1);
    /* Off the edge with nothing below: the gap warning takes over on the
     * same tick, the ledge warning stopping where it stood. */
    f.match_cues.tick(over_the_gap());
    assert(!f.engine.playing(left) && f.engine.playing(f.match_cues.gap_voice()));
    assert(count_logged_starting(
               "[a11y] cue ledge left stop pan -0.15 pitch 1.00 gain 1.00 interval 0.200") == 1);
    /* Near the right end, in the air. */
    f.match_cues.tick(near_the_edges(80.0f, true));
    assert(f.engine.playing(right) && !f.engine.playing(left));
    assert(count_logged_starting(
               "[a11y] cue ledge right start pan 0.27 pitch 1.00 gain 0.93 interval 0.200") == 1);
    /* Back in the middle: silent. */
    f.match_cues.tick(near_the_edges(0.0f, false));
    assert(!f.engine.playing(right));
    assert(count_logged_starting("[a11y] cue ledge right stop") == 1);
    /* Both ends of a small floor at once. */
    A11yMatchState small = near_the_edges(0.0f, false);
    small.below.left.x = -20.0f;
    small.below.right.x = 20.0f;
    f.match_cues.tick(small);
    assert(f.engine.playing(left) && f.engine.playing(right));
}

void the_ledge_pulse_sounds_on_its_side_five_times_a_second() {
    Fixture f;
    f.model.set(*f.model.find("your_position.on"), 0.0f);
    f.model.set(*f.model.find("opponent_position.on"), 0.0f);
    f.cues.apply(f.engine);
    f.match_cues.tick(near_the_edges(-85.6f, false));
    std::vector<float> out = render(f.engine, 1.0f);
    std::vector<double> at = onsets(out);
    assert(at.size() == 5);
    for (std::size_t i = 1; i < at.size(); ++i) {
        assert(std::fabs(at[i] - at[i - 1] - a11y::kLedgeInterval) < 0.002);
    }
    /* On its side by the constant-power pan at the pan floor. */
    double left = 0.0;
    double right = 0.0;
    energy(out, 0.0, left, right);
    const double pan = -f.model.find("ledge_warning.pan_floor")->default_value;
    const double angle = (pan + 1.0) * std::numbers::pi / 4.0;
    const double expected = std::pow(std::cos(angle) / std::sin(angle), 2.0);
    assert(left > right && std::fabs(left / right / expected - 1.0) < 0.01);
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

void one_switch_frames_both_pans() {
    for (const bool to_camera : {true, false}) {
        Fixture f;
        f.model.set(*f.model.find("position_cues.pan_to_camera"), to_camera ? 1.0f : 0.0f);
        A11yMatchState match = match_with(1, 85.0f);
        match.you.x = -85.0f;
        f.match_cues.tick(match);
        /* Final Destination's camera box sides are at ±170, its blast
         * zones at ±246. */
        if (to_camera) {
            assert(count_logged_starting("[a11y] cue your position start pan -0.50") == 1);
            assert(count_logged_starting("[a11y] cue opponent position start pan 0.50") == 1);
        } else {
            assert(count_logged_starting("[a11y] cue your position start pan -0.35") == 1);
            assert(count_logged_starting("[a11y] cue opponent position start pan 0.35") == 1);
        }
    }
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

/* The port menu opened mid-match (CueSystem::silence_for_port_menu, which
 * the tests cannot reach: it opens a device): every voice stops with its ramp
 * and the mix falls silent; the next tick starts the cues again from its snapshot
 * alone, so a gap warning holding on when the menu opened stays off. */
void silence_stops_every_voice_and_the_next_tick_starts_afresh() {
    Fixture f;
    f.match_cues.tick(over_the_gap());
    A11yMatchState landed = match_with(1, 60.0f);
    landed.below = A11yFloorBelow{true, 1, 0.0f};
    f.match_cues.tick(landed);
    const int gap = f.match_cues.gap_voice();
    assert(f.engine.playing(gap));
    render(f.engine, 0.1f);
    s_lines.clear();
    f.match_cues.silence();
    assert(!f.engine.playing(f.match_cues.your_voice()));
    assert(!f.engine.playing(f.match_cues.opponent_voice()));
    assert(!f.engine.playing(gap));
    assert(count_logged_starting("[a11y] cue your position stop") == 1);
    assert(count_logged_starting("[a11y] cue opponent position stop") == 1);
    assert(count_logged_starting("[a11y] cue gap stop") == 1);
    /* Past the stop ramp, nothing sounds. */
    std::vector<float> quiet = render(f.engine, 0.5f);
    const std::size_t past_ramp = static_cast<std::size_t>(0.01 * a11y::kCueRate) * 2;
    assert(std::all_of(quiet.begin() + static_cast<std::ptrdiff_t>(past_ramp), quiet.end(),
        [](float sample) { return sample == 0.0f; }));
    /* Silenced again, as each frame the menu stays open: nothing more. */
    f.match_cues.silence();
    assert(s_lines.size() == 3);
    /* The first tick after: both position cues start at once, pulsing
     * straight away, and the gap warning's hold is gone. */
    f.match_cues.tick(landed);
    assert(f.engine.playing(f.match_cues.your_voice()));
    assert(f.engine.playing(f.match_cues.opponent_voice()));
    assert(!f.engine.playing(gap));
    assert(count_logged_starting("[a11y] cue your position start") == 1);
    assert(count_logged_starting("[a11y] cue opponent position start") == 1);
    assert(count_logged_starting("[a11y] cue gap start") == 0);
    std::vector<double> at = onsets(render(f.engine, 0.1f));
    assert(!at.empty() && at[0] < 0.02);
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
    the_menus_rows_are_the_specs_rows();
    the_gap_warning_starts_holds_and_stops();
    the_gap_tone_loops_without_a_seam();
    the_ledge_warning_registers_last();
    the_ledge_warning_starts_on_each_side_and_stops();
    the_ledge_pulse_sounds_on_its_side_five_times_a_second();
    a_change_of_opponent_moves_the_voice_without_restarting_it();
    the_opponent_cue_stops_while_knocked_out_and_comes_back();
    one_switch_frames_both_pans();
    silence_stops_every_voice_and_the_next_tick_starts_afresh();
    std::cout << "match_cues: all tests passed\n";
    return 0;
}
