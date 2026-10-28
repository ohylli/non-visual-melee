/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Speech against a fake screen reader bridge. Links speech.cpp only, so this
 * file supplies the pc_log_line that main.c normally provides. */
#include "speech.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<std::string> s_lines;

struct Output {
    std::string text;
    bool interrupt;
};

/* Every call the fake bridge received; owned by the test, since Speech owns
 * the fake. Not to be confused with the speech log, captured in s_lines. */
struct BridgeCalls {
    int inits = 0;
    int shutdowns = 0;
    std::vector<Output> outputs;
};

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    FakeBridge(BridgeCalls& calls, std::string backend, std::string error)
        : m_calls(calls), m_backend(std::move(backend)), m_error(std::move(error)) {}

    std::string init() override {
        m_calls.inits++;
        return m_backend;
    }
    void shutdown() override { m_calls.shutdowns++; }
    std::string output(std::string_view utf8, bool interrupt) override {
        m_calls.outputs.push_back(Output{std::string(utf8), interrupt});
        return m_error;
    }

private:
    BridgeCalls& m_calls;
    std::string m_backend;
    std::string m_error;
};

/* A speech instance over a fresh fake bridge, and a fresh speech log: s_lines
 * only ever holds the lines of the test that is running. */
a11y::Speech make_speech(
    BridgeCalls& calls, a11y::Config config, std::string backend = "Fake", std::string error = "") {
    s_lines.clear();
    return a11y::Speech(
        config, std::make_unique<FakeBridge>(calls, std::move(backend), std::move(error)));
}

bool logged(const std::string& line) {
    for (const std::string& l : s_lines) {
        if (l == line) {
            return true;
        }
    }
    return false;
}

constexpr a11y::Config kOn{true, true};

void interrupt_and_queue_reach_bridge_in_order() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, kOn);
    speech.init();
    speech.announce("first", a11y::Mode::interrupt);
    speech.announce("second", a11y::Mode::queue);
    assert(calls.inits == 1);
    assert(calls.outputs.size() == 2);
    assert(calls.outputs[0].text == "first" && calls.outputs[0].interrupt);
    assert(calls.outputs[1].text == "second" && !calls.outputs[1].interrupt);
    speech.shutdown();
    assert(calls.shutdowns == 1);
}

void log_lines_have_the_documented_format() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, kOn, "NVDA");
    speech.init();
    speech.announce("hello", a11y::Mode::interrupt);
    speech.announce("Stock lost", a11y::Mode::queue);
    speech.shutdown();
    assert(s_lines.size() == 4);
    assert(s_lines[0] == "[a11y] speech backend: NVDA");
    assert(s_lines[1] == "[a11y] speak interrupt: \"hello\"");
    assert(s_lines[2] == "[a11y] speak queue: \"Stock lost\"");
    assert(s_lines[3] == "[a11y] speech shutdown");
}

void disabled_never_touches_bridge() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, a11y::Config{false, true});
    speech.init();
    speech.announce("hello", a11y::Mode::interrupt);
    speech.shutdown();
    assert(calls.inits == 0 && calls.shutdowns == 0 && calls.outputs.empty());
    assert(logged("[a11y] speak interrupt (off): \"hello\""));
    assert(logged("[a11y] speech shutdown"));
}

/* The switches turn speech on and off while the game runs: the screen
 * reader starts the first time speech is on, and once only; what was sent
 * before turning off was sent, and nothing after. */
void the_switch_is_live() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, a11y::Config{false, true});
    speech.set_on(true); /* before init: started by init */
    speech.set_on(false);
    speech.init();
    assert(calls.inits == 0 && !speech.enabled());
    speech.set_on(true);
    assert(calls.inits == 1 && speech.enabled());
    assert(logged("[a11y] speech backend: Fake"));
    speech.announce("Speech off", a11y::Mode::interrupt);
    speech.set_on(false);
    speech.announce("Off", a11y::Mode::interrupt);
    assert(calls.outputs.size() == 1 && calls.outputs[0].text == "Speech off");
    assert(logged("[a11y] speak interrupt (off): \"Off\""));
    assert(speech.last().text == "Off");
    speech.set_on(true);
    speech.announce("Speech on", a11y::Mode::interrupt);
    assert(calls.inits == 1 && calls.outputs.size() == 2);
    speech.shutdown();
    assert(calls.shutdowns == 1);
}

/* Accessibility stays on, so features act as for a player, but the screen
 * reader is never started or spoken to. */
void screen_reader_switch_keeps_accessibility_on() {
    BridgeCalls calls;
    a11y::Config config = kOn;
    config.screen_reader = false;
    a11y::Speech speech = make_speech(calls, config);
    speech.init();
    speech.announce("hello", a11y::Mode::interrupt);
    speech.shutdown();
    assert(speech.enabled());
    assert(calls.inits == 0 && calls.shutdowns == 0 && calls.outputs.empty());
    assert(logged("[a11y] screen reader off (MELEE_A11Y_SCREEN_READER=0)"));
    assert(logged("[a11y] speak interrupt (muted): \"hello\""));
    assert(logged("[a11y] speech shutdown"));
}

void log_switch_silences_log_only() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, a11y::Config{true, false});
    speech.init();
    speech.announce("hello", a11y::Mode::interrupt);
    speech.shutdown();
    assert(s_lines.empty());
    assert(calls.outputs.size() == 1);
}

void no_backend_still_calls_bridge() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, kOn, "");
    speech.init();
    speech.announce("hello", a11y::Mode::interrupt);
    assert(logged("[a11y] speech backend: none (silent)"));
    assert(calls.outputs.size() == 1);
}

void bridge_error_is_logged() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, kOn, "Fake", "boom");
    speech.init();
    speech.announce("hello", a11y::Mode::interrupt);
    assert(logged("[a11y] speak failed (boom): \"hello\""));
}

void last_is_most_recent_announcement() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, kOn);
    speech.init();
    speech.announce("first", a11y::Mode::interrupt);
    speech.announce("second", a11y::Mode::queue);
    assert(speech.last().text == "second" && speech.last().mode == a11y::Mode::queue);

    BridgeCalls off_calls;
    a11y::Speech off = make_speech(off_calls, a11y::Config{false, true});
    off.init();
    off.announce("unheard", a11y::Mode::interrupt);
    assert(off.last().text == "unheard" && off.last().mode == a11y::Mode::interrupt);
}

void call_before_init_is_dropped() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, kOn);
    speech.announce("early", a11y::Mode::interrupt);
    assert(calls.inits == 0 && calls.outputs.empty());
    assert(logged("[a11y] speech called before init, dropped: \"early\""));
    assert(speech.last().text.empty());
    speech.init();
    speech.announce("on time", a11y::Mode::interrupt);
    assert(calls.outputs.size() == 1);
}

void off_thread_call_is_dropped() {
    BridgeCalls calls;
    a11y::Speech speech = make_speech(calls, kOn);
    speech.init();
    speech.announce("mine", a11y::Mode::interrupt);
    assert(speech.initialized_on_this_thread());
    std::thread other([&speech] {
        assert(!speech.initialized_on_this_thread());
        speech.announce("theirs", a11y::Mode::queue);
    });
    other.join();
    assert(calls.outputs.size() == 1);
    assert(logged("[a11y] speech called off the game thread, dropped: \"theirs\""));
    assert(speech.last().text == "mine");
}

/* The voices as speech sees them, recording each call. */
class FakeVoices final : public a11y::VoiceLines {
public:
    void start() override { starts++; }
    void say(a11y::Voice voice, a11y::Subject subject, std::string_view text) override {
        said.push_back(std::string(a11y::voice_name(voice)) + " " + std::to_string(subject) + " " +
                       std::string(text));
    }
    void drop_waiting(const char* why) override { drops.push_back(why); }

    int starts = 0;
    std::vector<std::string> said;
    std::vector<std::string> drops;
};

/* A voice's line is logged naming its voice and reaches the voices while
 * speech is on; off, it is logged (off) alone. The voices start with the
 * screen reader, whatever its switch says, and turning speech off drops
 * their waiting lines. */
void voice_lines_are_logged_and_gated() {
    BridgeCalls calls;
    a11y::Config config = kOn;
    config.speech = false;
    a11y::Speech speech = make_speech(calls, config);
    FakeVoices voices;
    speech.attach_voices(&voices);
    speech.init();
    assert(voices.starts == 0);
    speech.announce_voice(a11y::Voice::yours, 1, "45");
    assert(logged("[a11y] voice yours (off): \"45\""));
    assert(voices.said.empty());
    speech.set_on(true);
    assert(voices.starts == 1);
    speech.announce_voice(a11y::Voice::opponents, 2, "P2 60");
    assert(logged("[a11y] voice opponents: \"P2 60\""));
    assert(voices.said.size() == 1 && voices.said[0] == "opponents 2 P2 60");
    /* Not the screen reader's: last() and the bridge are left alone. */
    assert(calls.outputs.empty() && speech.last().text.empty());
    speech.set_on(false);
    assert(voices.drops.size() == 1 && voices.drops[0] == std::string("speech off"));
    speech.set_on(true);
    assert(voices.starts == 1);
    speech.attach_voices(nullptr);
    speech.announce_voice(a11y::Voice::yours, 1, "gone");
    assert(logged("[a11y] voice yours dropped (no voices): \"gone\""));

    BridgeCalls muted_calls;
    a11y::Config muted = kOn;
    muted.screen_reader = false;
    a11y::Speech muted_speech = make_speech(muted_calls, muted);
    FakeVoices muted_voices;
    muted_speech.init();
    /* Attached after speech was on: started at once. */
    muted_speech.attach_voices(&muted_voices);
    assert(muted_voices.starts == 1);
    muted_speech.announce_voice(a11y::Voice::yours, a11y::kPreviewSubject, "Your voice");
    assert(logged("[a11y] voice yours: \"Your voice\""));
    assert(muted_voices.said.size() == 1);
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
    interrupt_and_queue_reach_bridge_in_order();
    log_lines_have_the_documented_format();
    disabled_never_touches_bridge();
    the_switch_is_live();
    screen_reader_switch_keeps_accessibility_on();
    log_switch_silences_log_only();
    no_backend_still_calls_bridge();
    bridge_error_is_logged();
    last_is_most_recent_announcement();
    call_before_init_is_dropped();
    off_thread_call_is_dropped();
    voice_lines_are_logged_and_gated();
    std::cout << "speech: all tests passed\n";
    return 0;
}
