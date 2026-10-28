/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The voices (ADR-0008, primer docs/a11y/speech.md, "Voices"): your voice
 * and the opponents' voice, each a Prism OneCore instance of its own that a
 * worker thread renders lines with, played by a once voice of the cue engine.
 * Speech logs each line and lets it through (Speech::announce_voice); this
 * queues it, has it rendered, and plays it in turn.
 *
 * Queueing, within one voice: a new line about a subject whose line is
 * playing cuts that line off, with the engine's stop ramp, as soon as the new
 * one is rendered, and plays before anything waiting; one about a subject
 * whose line is waiting takes that line's place; any other waits behind the
 * lines already there. The two voices never wait for or cut each other, nor
 * the screen reader.
 *
 * Threads: everything here is the game thread's but the worker, which owns
 * the bridge: it opens it, renders each line, evens it out and converts it
 * for the cue engine, and hands it back through a locked queue the game
 * thread empties each frame. The worker never logs: its news is logged by the
 * game thread. */
#pragma once
#include "screen_reader_bridge.hpp"
#include "speech.hpp"
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace a11y {

using Clock = std::chrono::steady_clock;

class CueEngine;
struct Sound;

enum class VoicesState {
    stopped,     /* speech has not been on yet: no worker */
    starting,    /* the worker is opening the instances */
    ready,       /* lines are rendered and played */
    unavailable, /* OneCore did not start, or its test line failed: lines are dropped */
};

/* The loudest sample of every line once evened out, before the cue engine's
 * chain: OneCore's loudness varies from word to word. A tuning value. */
constexpr float kVoicePeak = 0.8f;

/* The voice a setting's text names among the installed: the one of that
 * display name, ignoring case; else the first whose name holds the text,
 * ignoring case ("Zira"); else default_voice. Empty text is the default. -1
 * when none is installed. */
int find_voice(const std::vector<VoiceInfo>& voices, std::string_view wanted, int default_voice);

/* Sorts voices by language and then name, as the menus list them, and
 * returns each one's index in the order they were given. */
std::vector<int> sort_voices(std::vector<VoiceInfo>& voices);

/* A rendered line made ready for the cue engine: mono at kCueRate, its peak
 * at kVoicePeak. Empty when the line holds no sound. Any thread. */
Sound even_out(const RenderedLine& line);

class Voices final : public VoiceLines {
public:
    /* Adds a once voice per voice to the engine, which must outlive this,
     * and attaches to speech, which starts the worker the first time it is
     * on. preview is MELEE_A11Y_VOICE_PREVIEW. Your voice wants "Zira" and
     * the opponents' voice "Mark" until set_voice says otherwise. */
    Voices(Speech& speech, CueEngine& engine, std::unique_ptr<VoiceBridge> bridge, bool log,
        bool preview);
    /* Detaches from speech, stops the voices and joins the worker, which
     * closes the bridge on its own thread. */
    ~Voices() override;
    Voices(const Voices&) = delete;
    Voices& operator=(const Voices&) = delete;

    void start() override;
    void say(Voice voice, Subject subject, std::string_view text) override;
    void drop_waiting(const char* why) override;

    /* Each frame, from the launcher's frame hook and from the port menu's,
     * which runs at every frame boundary, outside the simulation: the
     * worker's news logged, rendered lines played in turn, the lines waiting
     * dropped as the port menu opens, and MELEE_A11Y_VOICE_PREVIEW's lines. */
    void frame(Clock::time_point now, bool port_menu_open);

    VoicesState state() const { return m_state; }
    /* The installed voices, sorted by language and then name; empty until
     * ready. Kept, so a menu can copy it every frame. */
    const std::vector<VoiceInfo>& installed() const { return m_installed; }
    /* The Windows default voice's display name; empty until ready or when
     * unknown. */
    const std::string& default_voice() const { return m_default; }
    /* A setting's text, a display name or part of one: the voice the next
     * line rendered speaks with (find_voice). */
    void set_voice(Voice voice, std::string_view wanted);
    /* The display name find_voice gives for the voice's text; empty until
     * ready. */
    std::string voice_in_use(Voice voice) const;
    /* 0 to 1, 0.5 the voice's normal speed: from the next line rendered. */
    void set_rate(Voice voice, float rate);
    /* 0 to 1, at once, the line playing included: the voice's factor of
     * the cue engine's chain, under the master volume. */
    void set_volume(Voice voice, float volume);
    /* A line in this voice plays or waits to. */
    bool speaking(Voice voice) const;

    /* For tests: blocks until the worker has opened the bridge and rendered
     * every line asked for so far. */
    void wait_for_worker();

private:
    struct Line {
        int id = 0;
        Subject subject = 0;
        std::string text;
        Clock::time_point said;
        /* About the subject of the line playing: plays once rendered,
         * cutting it off. */
        bool cuts = false;
        bool rendered = false;
        std::vector<float> samples;
        double render_ms = 0.0;
    };
    struct Playing {
        bool on = false;
        Subject subject = 0;
        std::string text;
    };
    struct VoiceSlot {
        int engine_voice = -1;
        std::string wanted;
        float rate = 0.5f;
        std::deque<Line> waiting;
        Playing playing;
    };
    /* A line for the worker to render. */
    struct Job {
        int voice = 0;
        int id = 0;
        std::string text;
        std::string wanted;
        float rate = 0.5f;
    };
    /* The worker's answer to a job. */
    struct Rendered {
        int voice = 0;
        int id = 0;
        std::vector<float> samples;
        std::string error;
        double ms = 0.0;
    };

    VoiceSlot& slot(Voice voice) { return m_voices[static_cast<size_t>(voice)]; }
    const VoiceSlot& slot(Voice voice) const { return m_voices[static_cast<size_t>(voice)]; }
    void ask_render(int voice, const Line& line);
    void take_news();
    void opened(
        const std::string& error, std::vector<VoiceInfo> listed, int default_index, double test_ms);
    void advance(int voice);
    void preview_frame(Clock::time_point now);
    void drop(int voice, const Line& line, const char* why) const;
    void log(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));
    void work();

    Speech& m_speech;
    CueEngine& m_engine;
    bool m_log;
    VoicesState m_state = VoicesState::stopped;
    std::array<VoiceSlot, kVoiceCount> m_voices;
    std::vector<VoiceInfo> m_installed;
    std::string m_default;
    int m_default_index = -1; /* in m_installed */
    int m_next_id = 1;
    bool m_port_menu_open = false;

    /* MELEE_A11Y_VOICE_PREVIEW: 0 not yet, 1 your voice asked, 2 waiting
     * for the pause, 3 done. */
    bool m_preview;
    int m_preview_step = 0;
    Clock::time_point m_preview_at;

    /* The worker's own: the bridge, and the voices sorted, with the default's
     * index among them. */
    std::unique_ptr<VoiceBridge> m_bridge;
    std::vector<VoiceInfo> m_worker_voices;
    int m_worker_default = -1;

    /* Shared with the worker, under m_mutex. */
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::condition_variable m_idle;
    std::deque<Job> m_jobs;
    std::vector<Rendered> m_rendered;
    bool m_quit = false;
    bool m_busy = false;
    bool m_open_done = false;
    bool m_open_seen = false;
    std::string m_open_error;
    std::vector<VoiceInfo> m_listed;
    int m_listed_default = -1;
    double m_test_ms = 0.0;

    std::thread m_worker;
};

}  // namespace a11y
