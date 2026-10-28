/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The voices against a fake voice bridge, with speech over a fake screen
 * reader and the cue engine rendered into buffers, a frame at a time on a
 * fake clock. The worker is the real one: each step waits for it, so what it
 * renders is back before the frame that plays it. This file supplies the
 * pc_log_line that main.c normally provides. */
#include "cue_engine.hpp"
#include "speech.hpp"
#include "voices.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace {

using a11y::Clock;
using a11y::CueEngine;
using a11y::kCueRate;
using a11y::Voice;
using a11y::Voices;
using a11y::VoicesState;

std::vector<std::string> s_lines;

class NoScreenReader final : public a11y::ScreenReaderBridge {
public:
    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view, bool) override { return {}; }
};

/* What the fake voice bridge was asked, from the worker thread. */
struct VoiceCalls {
    std::mutex mutex;
    int opens = 0;
    int closes = 0;
    std::vector<std::pair<int, int>> set_voices; /* instance, bridge index */
    std::vector<std::pair<int, float>> set_rates;
    std::vector<std::pair<int, std::string>> renders;
};

/* Renders each line as 0.1 s per character of a stereo constant at 16 kHz,
 * quieter on the right; a line of "silence" renders silence, and "fail"
 * fails. */
class FakeVoiceBridge final : public a11y::VoiceBridge {
public:
    FakeVoiceBridge(VoiceCalls& calls, std::string open_error)
        : m_calls(calls), m_open_error(std::move(open_error)) {}

    std::string open(int count) override {
        std::lock_guard<std::mutex> lock(m_calls.mutex);
        m_calls.opens++;
        assert(count == a11y::kVoiceCount);
        return m_open_error;
    }
    void close() override {
        std::lock_guard<std::mutex> lock(m_calls.mutex);
        m_calls.closes++;
    }
    std::vector<a11y::VoiceInfo> voices() override {
        return {{"Microsoft David", "en-US"}, {"Microsoft George", "en-GB"},
            {"Microsoft Zira", "en-US"}, {"Microsoft Heidi", "fi-FI"}, {"Microsoft Mark", "en-US"}};
    }
    int default_voice() override { return 1; }
    std::string set_voice(int instance, int index) override {
        std::lock_guard<std::mutex> lock(m_calls.mutex);
        m_calls.set_voices.emplace_back(instance, index);
        return {};
    }
    std::string set_rate(int instance, float rate) override {
        std::lock_guard<std::mutex> lock(m_calls.mutex);
        m_calls.set_rates.emplace_back(instance, rate);
        return {};
    }
    std::string render(int instance, std::string_view utf8, a11y::RenderedLine& out) override {
        {
            std::lock_guard<std::mutex> lock(m_calls.mutex);
            m_calls.renders.emplace_back(instance, std::string(utf8));
        }
        if (utf8 == "fail") {
            return "boom";
        }
        const float level = utf8 == "silence" ? 0.0f : 0.3f;
        const size_t frames = utf8.size() * 1600;
        out.channels = 2;
        out.rate = 16000;
        out.samples.clear();
        for (size_t f = 0; f < frames; f++) {
            out.samples.push_back(level);
            out.samples.push_back(level / 3.0f);
        }
        return {};
    }

private:
    VoiceCalls& m_calls;
    std::string m_open_error;
};

/* Speech, the cue engine and the voices, wired as hooks.cpp wires them. */
struct Rig {
    explicit Rig(VoiceCalls& calls, std::string open_error = "", bool preview = false)
        : speech(config(), std::make_unique<NoScreenReader>()),
          voices(speech, engine, std::make_unique<FakeVoiceBridge>(calls, std::move(open_error)),
              true, preview) {
        s_lines.clear();
        speech.init();
        frames(1);
    }

    static a11y::Config config() {
        a11y::Config c;
        c.speech = true;
        return c;
    }

    /* n frames of 1/60 s: the worker's renders back, a frame of the voices,
     * then the engine's audio for the frame. */
    std::vector<float> frames(int n, bool port_menu = false) {
        std::vector<float> out;
        for (int i = 0; i < n; i++) {
            voices.wait_for_worker();
            voices.frame(now, port_menu);
            std::vector<float> block(800 * 2);
            engine.render(block.data(), 800);
            out.insert(out.end(), block.begin(), block.end());
            now += std::chrono::microseconds(16667);
        }
        return out;
    }

    /* Frames until neither voice speaks, at most ten seconds. */
    void play_out() {
        for (int i = 0;
            i < 600 && (voices.speaking(Voice::yours) || voices.speaking(Voice::opponents)); i++)
        {
            frames(1);
        }
    }

    void yours(int subject, const char* text) {
        speech.announce_voice(Voice::yours, subject, text);
    }
    void theirs(int subject, const char* text) {
        speech.announce_voice(Voice::opponents, subject, text);
    }

    CueEngine engine;
    a11y::Speech speech;
    Voices voices;
    Clock::time_point now{};
};

bool starts_with(const std::string& line, const std::string& start) {
    return line.compare(0, start.size(), start) == 0;
}

/* The index of the first log line starting so, or -1. */
int line_at(const std::string& start) {
    for (size_t i = 0; i < s_lines.size(); i++) {
        if (starts_with(s_lines[i], start)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool logged(const std::string& start) {
    return line_at(start) != -1;
}

/* The lines that start so, in order: the voices' plays, cuts, drops. */
std::vector<std::string> lines_starting(const std::string& start) {
    std::vector<std::string> found;
    for (const std::string& line : s_lines) {
        if (starts_with(line, start)) {
            found.push_back(line);
        }
    }
    return found;
}

std::string plays(const char* voice, const char* text) {
    return std::string("[a11y] voice ") + voice + " plays: \"" + text + "\"";
}

float peak(const std::vector<float>& out) {
    float largest = 0.0f;
    for (float s : out) {
        largest = std::max(largest, std::fabs(s));
    }
    return largest;
}

void the_voices_open_with_their_defaults() {
    VoiceCalls calls;
    Rig rig(calls);
    assert(rig.voices.state() == VoicesState::ready);
    assert(logged("[a11y] voices: OneCore, 5 installed, default Microsoft George, test line "
                  "rendered in "));
    assert(logged("[a11y] voice yours speaks with Microsoft Zira"));
    assert(logged("[a11y] voice opponents speaks with Microsoft Mark"));
    /* By language, then name. */
    const std::vector<a11y::VoiceInfo>& installed = rig.voices.installed();
    assert(installed.size() == 5);
    assert(installed[0].name == "Microsoft George" && installed[1].name == "Microsoft David");
    assert(installed[2].name == "Microsoft Mark" && installed[3].name == "Microsoft Zira");
    assert(installed[4].name == "Microsoft Heidi");
    assert(rig.voices.default_voice() == "Microsoft George");
    /* The first line in each voice sets its instance's voice, by the
     * bridge's own index. */
    rig.yours(1, "45");
    rig.theirs(2, "60");
    rig.frames(1);
    std::lock_guard<std::mutex> lock(calls.mutex);
    assert(calls.opens == 1);
    assert(std::find(calls.set_voices.begin(), calls.set_voices.end(), std::make_pair(0, 2)) !=
           calls.set_voices.end());
    assert(std::find(calls.set_voices.begin(), calls.set_voices.end(), std::make_pair(1, 4)) !=
           calls.set_voices.end());
}

void a_voice_is_found_by_name_or_part() {
    std::vector<a11y::VoiceInfo> voices = {
        {"Microsoft George", "en-GB"}, {"Microsoft David", "en-US"}, {"Microsoft Zira", "en-US"}};
    assert(a11y::find_voice(voices, "Microsoft Zira", 0) == 2);
    assert(a11y::find_voice(voices, "microsoft zira", 0) == 2);
    assert(a11y::find_voice(voices, "zira", 0) == 2);
    assert(a11y::find_voice(voices, "Microsoft", 1) == 0);
    assert(a11y::find_voice(voices, "Hazel", 1) == 1);
    assert(a11y::find_voice(voices, "", 1) == 1);
    assert(a11y::find_voice(voices, "Hazel", -1) == 0);
    assert(a11y::find_voice({}, "Zira", 0) == -1);

    VoiceCalls calls;
    Rig rig(calls);
    rig.voices.set_voice(Voice::yours, "Hazel");
    assert(rig.voices.voice_in_use(Voice::yours) == "Microsoft George");
    rig.voices.set_voice(Voice::yours, "microsoft david");
    assert(rig.voices.voice_in_use(Voice::yours) == "Microsoft David");
    rig.yours(1, "45");
    rig.frames(1);
    std::lock_guard<std::mutex> lock(calls.mutex);
    assert(calls.set_voices.back() == std::make_pair(0, 0));
}

void a_rate_applies_from_the_next_line() {
    VoiceCalls calls;
    Rig rig(calls);
    rig.yours(1, "45");
    rig.voices.set_rate(Voice::opponents, 0.8f);
    rig.theirs(2, "60");
    rig.frames(1);
    std::lock_guard<std::mutex> lock(calls.mutex);
    /* Your voice was at the normal rate already. */
    assert(calls.set_rates.size() == 1);
    assert(calls.set_rates[0] == std::make_pair(1, 0.8f));
}

/* Lines about different subjects wait their turn, each played whole. */
void lines_queue_in_turn() {
    VoiceCalls calls;
    Rig rig(calls);
    rig.yours(1, "one");
    rig.yours(2, "two");
    rig.yours(3, "three");
    assert(logged("[a11y] voice yours: \"one\""));
    rig.frames(1);
    assert(lines_starting("[a11y] voice yours plays").size() == 1);
    assert(logged(plays("yours", "one")));
    rig.play_out();
    const std::vector<std::string> played = lines_starting("[a11y] voice yours plays");
    assert(played.size() == 3);
    assert(starts_with(played[1], plays("yours", "two")));
    assert(starts_with(played[2], plays("yours", "three")));
    assert(!logged("[a11y] voice yours cut"));
    assert(!logged("[a11y] voice yours dropped"));
}

/* A line about the subject that plays cuts it off and plays at once, ahead
 * of the lines waiting. */
void a_line_about_the_same_subject_cuts_the_one_playing() {
    VoiceCalls calls;
    Rig rig(calls);
    rig.yours(1, "45 percent");
    rig.yours(2, "other");
    rig.frames(5);
    rig.yours(1, "60");
    rig.frames(1);
    int cut = line_at("[a11y] voice yours cut: \"45 percent\"");
    int sixty = line_at(plays("yours", "60"));
    assert(cut != -1 && sixty == cut + 1);
    assert(!logged(plays("yours", "other")));
    rig.play_out();
    assert(line_at(plays("yours", "other")) > sixty);
    assert(!logged("[a11y] voice yours dropped"));
}

/* A line about a subject whose line waits takes its place in the queue. */
void a_line_about_a_waiting_subject_takes_its_place() {
    VoiceCalls calls;
    Rig rig(calls);
    rig.yours(1, "first");
    rig.yours(2, "stale");
    rig.yours(3, "third");
    rig.frames(1);
    rig.yours(2, "fresh");
    assert(logged("[a11y] voice yours dropped (replaced): \"stale\""));
    rig.play_out();
    const std::vector<std::string> played = lines_starting("[a11y] voice yours plays");
    assert(played.size() == 3);
    assert(starts_with(played[1], plays("yours", "fresh")));
    assert(starts_with(played[2], plays("yours", "third")));
    assert(!logged("[a11y] voice yours cut"));
}

/* The two voices never wait for or cut each other. */
void the_voices_are_independent() {
    VoiceCalls calls;
    Rig rig(calls);
    rig.yours(1, "a long line of yours");
    rig.frames(2);
    rig.theirs(1, "theirs");
    rig.frames(1);
    assert(logged(plays("opponents", "theirs")));
    assert(rig.engine.playing(0) && rig.engine.playing(1));
    /* A cut in one leaves the other playing. */
    rig.yours(1, "cut");
    rig.frames(1);
    assert(logged("[a11y] voice yours cut: \"a long line of yours\""));
    assert(!logged("[a11y] voice opponents cut"));
    assert(rig.voices.speaking(Voice::opponents));
}

/* Speech off: the line playing finishes, the waiting ones are dropped, and
 * new ones are only logged. */
void speech_off_drops_the_waiting_lines() {
    VoiceCalls calls;
    Rig rig(calls);
    rig.yours(1, "playing");
    rig.yours(2, "waiting");
    rig.theirs(3, "theirs");
    rig.frames(1);
    rig.speech.set_on(false);
    assert(logged("[a11y] voice yours dropped (speech off): \"waiting\""));
    assert(!logged("[a11y] voice opponents dropped"));
    rig.yours(4, "unheard");
    assert(logged("[a11y] voice yours (off): \"unheard\""));
    rig.frames(1);
    assert(rig.engine.playing(0) && rig.engine.playing(1));
    rig.play_out();
    assert(!logged(plays("yours", "waiting")));
    assert(!logged(plays("yours", "unheard")));
    rig.speech.set_on(true);
    rig.yours(5, "back");
    rig.frames(1);
    assert(logged(plays("yours", "back")));
}

/* The port menu opening does the same; a line said with it open, a
 * preview, plays. */
void the_port_menu_drops_the_waiting_lines() {
    VoiceCalls calls;
    Rig rig(calls);
    rig.yours(1, "playing");
    rig.yours(2, "waiting");
    rig.frames(1);
    rig.frames(1, true);
    assert(logged("[a11y] voice yours dropped (port menu): \"waiting\""));
    assert(rig.engine.playing(0));
    rig.yours(a11y::kPreviewSubject, "Your voice");
    rig.frames(1, true);
    assert(logged("[a11y] voice yours cut: \"playing\"") == false);
    rig.frames(60, true);
    assert(logged(plays("yours", "Your voice")));
    /* Still open: nothing more is dropped. */
    rig.yours(3, "queued");
    rig.frames(1, true);
    assert(!logged("[a11y] voice yours dropped (port menu): \"queued\""));
}

/* Every line reaches the same peak, then the voice's volume and the master
 * volume apply. */
void lines_are_evened_out() {
    a11y::RenderedLine line;
    line.channels = 2;
    line.rate = 16000;
    for (int f = 0; f < 1600; f++) {
        line.samples.push_back(f == 800 ? 0.2f : 0.05f);
        line.samples.push_back(0.05f);
    }
    a11y::Sound sound = a11y::even_out(line);
    assert(sound.samples.size() == 4800);
    assert(std::fabs(peak(sound.samples) - a11y::kVoicePeak) < 1e-6f);
    a11y::RenderedLine quiet = line;
    for (float& s : quiet.samples) {
        s *= 0.1f;
    }
    assert(std::fabs(peak(a11y::even_out(quiet).samples) - a11y::kVoicePeak) < 1e-6f);
    a11y::RenderedLine silent;
    silent.channels = 1;
    silent.rate = 48000;
    silent.samples.assign(480, 0.0f);
    assert(a11y::even_out(silent).samples.empty());

    VoiceCalls calls;
    Rig rig(calls);
    /* The mix's headroom and a centred pan. */
    const float centred = a11y::kVoicePeak * 0.5f * std::sqrt(0.5f);
    rig.yours(1, "a long line to measure");
    std::vector<float> out = rig.frames(30);
    assert(std::fabs(peak(out) - centred) < 0.01f);
    rig.voices.set_volume(Voice::yours, 0.5f);
    rig.frames(5);
    out = rig.frames(10);
    assert(std::fabs(peak(out) - 0.5f * centred) < 0.01f);
    rig.engine.set_master_volume(0.5f);
    rig.frames(5);
    out = rig.frames(10);
    assert(std::fabs(peak(out) - 0.25f * centred) < 0.01f);
    /* The cue volume and switches leave it alone. */
    rig.engine.set_mix_level(a11y::CueLevel{false, 0.1f, 1.0f});
    out = rig.frames(10);
    assert(std::fabs(peak(out) - 0.25f * centred) < 0.01f);
    /* A line of silence is not played. */
    rig.play_out();
    rig.yours(2, "silence");
    rig.frames(1);
    assert(logged("[a11y] voice yours dropped (no sound rendered): \"silence\""));
}

void a_failed_render_is_dropped() {
    VoiceCalls calls;
    Rig rig(calls);
    rig.yours(1, "fail");
    rig.yours(2, "next");
    rig.frames(1);
    assert(logged("[a11y] voice yours dropped (render failed: boom): \"fail\""));
    assert(logged(plays("yours", "next")));
}

void unavailable_voices_drop_their_lines() {
    VoiceCalls calls;
    a11y::Config config = Rig::config();
    a11y::Speech speech(config, std::make_unique<NoScreenReader>());
    CueEngine engine;
    s_lines.clear();
    {
        Voices voices(speech, engine,
            std::make_unique<FakeVoiceBridge>(calls, "OneCore did not start"), true, false);
        speech.init();
        assert(voices.state() == VoicesState::starting);
        speech.announce_voice(Voice::yours, 1, "early");
        voices.wait_for_worker();
        voices.frame(Clock::time_point{}, false);
        assert(voices.state() == VoicesState::unavailable);
        assert(logged("[a11y] voices unavailable: OneCore did not start"));
        assert(logged("[a11y] voice yours dropped (voices unavailable): \"early\""));
        speech.announce_voice(Voice::opponents, 1, "late");
        assert(logged("[a11y] voice opponents dropped (voices unavailable): \"late\""));
    }
    std::lock_guard<std::mutex> lock(calls.mutex);
    assert(calls.closes == 1);
}

/* The worker starts the first time speech is on, and never before. */
void the_worker_starts_with_speech() {
    VoiceCalls calls;
    a11y::Config config = Rig::config();
    config.speech = false;
    a11y::Speech speech(config, std::make_unique<NoScreenReader>());
    CueEngine engine;
    Voices voices(speech, engine, std::make_unique<FakeVoiceBridge>(calls, ""), true, false);
    speech.init();
    assert(voices.state() == VoicesState::stopped);
    speech.set_on(true);
    assert(voices.state() == VoicesState::starting);
    voices.wait_for_worker();
    voices.frame(Clock::time_point{}, false);
    assert(voices.state() == VoicesState::ready);
    /* With the screen reader switched off too: the voices play through the
     * cue engine. */
    VoiceCalls muted_calls;
    a11y::Config muted = Rig::config();
    muted.screen_reader = false;
    a11y::Speech muted_speech(muted, std::make_unique<NoScreenReader>());
    CueEngine muted_engine;
    Voices muted_voices(muted_speech, muted_engine,
        std::make_unique<FakeVoiceBridge>(muted_calls, ""), true, false);
    s_lines.clear();
    muted_speech.init();
    muted_speech.announce_voice(Voice::yours, 1, "45");
    muted_voices.wait_for_worker();
    muted_voices.frame(Clock::time_point{}, false);
    assert(logged("[a11y] voice yours: \"45\""));
    assert(logged(plays("yours", "45")));
}

/* MELEE_A11Y_VOICE_PREVIEW: your voice, then after a pause the opponents'
 * voice, through speech and its log. */
void the_developer_preview_says_each_voice() {
    VoiceCalls calls;
    Rig rig(calls, "", true);
    assert(logged("[a11y] voice yours: \"Your voice\""));
    rig.frames(1);
    assert(logged(plays("yours", "Your voice")));
    assert(!logged("[a11y] voice opponents: "));
    rig.play_out();
    rig.frames(60);
    assert(logged("[a11y] voice opponents: \"Opponents' voice\""));
    rig.play_out();
    assert(logged(plays("opponents", "Opponents' voice")));
    assert(lines_starting("[a11y] voice yours plays").size() == 1);
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

int main() {
    the_voices_open_with_their_defaults();
    a_voice_is_found_by_name_or_part();
    a_rate_applies_from_the_next_line();
    lines_queue_in_turn();
    a_line_about_the_same_subject_cuts_the_one_playing();
    a_line_about_a_waiting_subject_takes_its_place();
    the_voices_are_independent();
    speech_off_drops_the_waiting_lines();
    the_port_menu_drops_the_waiting_lines();
    lines_are_evened_out();
    a_failed_render_is_dropped();
    unavailable_voices_drop_their_lines();
    the_worker_starts_with_speech();
    the_developer_preview_says_each_voice();
    std::cout << "voices: all tests passed\n";
    return 0;
}
