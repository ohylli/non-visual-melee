/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A fake voice bridge for the voices' tests (test_voices.cpp,
 * test_voice_settings.cpp): what the worker asks of it recorded, and lines
 * rendered as known samples; with turns needed, each call waits for the
 * game thread's turn, as AVSpeech's wait for the Mac's main thread. */
#pragma once
#include "screen_reader_bridge.hpp"
#include "speech.hpp"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace a11y::test {

/* What the fake voice bridge was asked, from the worker thread. */
struct VoiceCalls {
    std::mutex mutex;
    int opens = 0;
    int closes = 0;
    std::vector<std::pair<int, int>> set_voices; /* instance, bridge index */
    std::vector<std::pair<int, float>> set_rates;
    std::vector<std::pair<int, std::string>> renders;

    /* Set before the voices start: every call the worker makes waits for a
     * turn (VoiceBridge::turn) given from another thread, at most
     * kTurnTimeout, as each of AVSpeech's waits for the Mac's main thread. */
    bool needs_turns = false;
    std::condition_variable turned;
    int asked = 0; /* turns the worker has waited for */
    int given = 0; /* of those, given */
    int timeouts = 0;
    int wrong_thread_turns = 0; /* turns given from the worker itself */
};

/* How long a call waits for its turn before it counts a timeout and goes
 * on, so a test without the turns fails rather than hangs. */
constexpr std::chrono::seconds kTurnTimeout{2};

/* The voices' backend the tests give the voices and their rows, under a
 * name of the tests' own. */
constexpr VoiceBackend kTestBackend{"Fake voices", {{{"Zira", "en-US"}, {"Mark", "en-US"}}}};

/* Five voices in three languages, the backend's default the second. */
inline std::vector<VoiceInfo> several_languages() {
    return {{"Microsoft David", "en-US"}, {"Microsoft George", "en-GB"},
        {"Microsoft Zira", "en-US"}, {"Microsoft Heidi", "fi-FI"}, {"Microsoft Mark", "en-US"}};
}

/* Voices whose names repeat across languages, as on a Mac, one with
 * brackets of its own, sorted as the voices sort them; the backend's
 * default the first. */
inline std::vector<VoiceInfo> repeated_names() {
    return {{"Eddy", "de-DE"}, {"Eddy (English (UK))", "en-GB"}, {"Eddy", "en-US"},
        {"Zira Lite", "en-US"}, {"Eddy", "fi-FI"}, {"Zira", "fi-FI"}};
}

/* Renders each line as 0.1 s per character of a stereo constant at 16 kHz,
 * quieter on the right; a line of "silence" renders silence, and "fail"
 * fails. */
class FakeVoiceBridge final : public VoiceBridge {
public:
    FakeVoiceBridge(VoiceCalls& calls, std::string open_error,
        std::vector<VoiceInfo> voices = several_languages(), int default_voice = 1)
        : m_calls(calls), m_open_error(std::move(open_error)), m_voices(std::move(voices)),
          m_default(default_voice) {}

    std::string open(int count) override {
        std::unique_lock<std::mutex> lock(m_calls.mutex);
        m_worker = std::this_thread::get_id();
        wait_for_turn(lock);
        m_calls.opens++;
        assert(count == kVoiceCount);
        return m_open_error;
    }
    void close() override {
        std::unique_lock<std::mutex> lock(m_calls.mutex);
        wait_for_turn(lock);
        m_calls.closes++;
    }
    std::vector<VoiceInfo> voices() override {
        std::unique_lock<std::mutex> lock(m_calls.mutex);
        wait_for_turn(lock);
        return m_voices;
    }
    int default_voice() override {
        std::unique_lock<std::mutex> lock(m_calls.mutex);
        wait_for_turn(lock);
        return m_default;
    }
    std::string set_voice(int instance, int index) override {
        std::unique_lock<std::mutex> lock(m_calls.mutex);
        wait_for_turn(lock);
        m_calls.set_voices.emplace_back(instance, index);
        return {};
    }
    std::string set_rate(int instance, float rate) override {
        std::unique_lock<std::mutex> lock(m_calls.mutex);
        wait_for_turn(lock);
        m_calls.set_rates.emplace_back(instance, rate);
        return {};
    }
    std::string render(int instance, std::string_view utf8, RenderedLine& out) override {
        {
            std::unique_lock<std::mutex> lock(m_calls.mutex);
            m_calls.renders.emplace_back(instance, std::string(utf8));
            wait_for_turn(lock);
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

    /* Touches the turn's own counts alone, never what the calls record. */
    void turn() override {
        std::lock_guard<std::mutex> lock(m_calls.mutex);
        if (std::this_thread::get_id() == m_worker) {
            m_calls.wrong_thread_turns++;
        }
        if (m_calls.given < m_calls.asked) {
            m_calls.given = m_calls.asked;
            m_calls.turned.notify_all();
        }
    }

private:
    /* On the worker, under the calls' lock: waits for the game thread's
     * turn when turns are needed. */
    void wait_for_turn(std::unique_lock<std::mutex>& lock) {
        if (!m_calls.needs_turns) {
            return;
        }
        const int mine = ++m_calls.asked;
        m_calls.turned.notify_all(); /* for a test waiting for the ask */
        if (!m_calls.turned.wait_for(
                lock, kTurnTimeout, [this, mine] { return m_calls.given >= mine; }))
        {
            m_calls.timeouts++;
        }
    }

    VoiceCalls& m_calls;
    std::string m_open_error;
    std::vector<VoiceInfo> m_voices;
    int m_default;
    std::thread::id m_worker; /* under the calls' lock */
};

}  // namespace a11y::test
