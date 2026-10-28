/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A fake voice bridge for the voices' tests (test_voices.cpp,
 * test_voice_settings.cpp): what the worker asks of it recorded, and lines
 * rendered as known samples. */
#pragma once
#include "screen_reader_bridge.hpp"
#include "speech.hpp"
#include <cassert>
#include <mutex>
#include <string>
#include <string_view>
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
};

/* Five voices in three languages, the Windows default the second. */
inline std::vector<VoiceInfo> several_languages() {
    return {{"Microsoft David", "en-US"}, {"Microsoft George", "en-GB"},
        {"Microsoft Zira", "en-US"}, {"Microsoft Heidi", "fi-FI"}, {"Microsoft Mark", "en-US"}};
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
        std::lock_guard<std::mutex> lock(m_calls.mutex);
        m_calls.opens++;
        assert(count == kVoiceCount);
        return m_open_error;
    }
    void close() override {
        std::lock_guard<std::mutex> lock(m_calls.mutex);
        m_calls.closes++;
    }
    std::vector<VoiceInfo> voices() override { return m_voices; }
    int default_voice() override { return m_default; }
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
    std::string render(int instance, std::string_view utf8, RenderedLine& out) override {
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
    std::vector<VoiceInfo> m_voices;
    int m_default;
};

}  // namespace a11y::test
