/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "voices.hpp"
#include "cue_engine.hpp"
#include "miniaudio.h"
#include "pc/pc.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <utility>

namespace a11y {
namespace {

/* The line the worker renders as the voices open: a failure here is OneCore
 * unavailable, and its time is the first render's, the slowest. */
constexpr const char* kTestLine = "Ready";

/* MELEE_A11Y_VOICE_PREVIEW's pause between your voice's line ending and the
 * opponents' voice's, so the dump shows each apart. */
constexpr std::chrono::milliseconds kPreviewPause{700};

/* Below this a line's peak is silence, left as it is rather than raised. */
constexpr float kSilentPeak = 1e-4f;

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool same_text(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                       [](char x, char y) { return lower(x) == lower(y); });
}

bool holds(std::string_view text, std::string_view part) {
    return std::search(text.begin(), text.end(), part.begin(), part.end(),
               [](char x, char y) { return lower(x) == lower(y); }) != text.end();
}

int text_length(std::string_view text) {
    return static_cast<int>(text.size());
}

double ms_between(Clock::time_point from, Clock::time_point to) {
    return std::chrono::duration<double, std::milli>(to - from).count();
}

/* Mono at the line's own rate: the channels averaged. */
std::vector<float> downmix(const RenderedLine& line) {
    const size_t channels = static_cast<size_t>(std::max(line.channels, 1));
    const size_t frames = line.samples.size() / channels;
    std::vector<float> mono(frames);
    for (size_t f = 0; f < frames; f++) {
        float sum = 0.0f;
        for (size_t c = 0; c < channels; c++) {
            sum += line.samples[f * channels + c];
        }
        mono[f] = sum / static_cast<float>(channels);
    }
    return mono;
}

/* To kCueRate with miniaudio's linear resampler and its low-pass filter, which
 * keeps the images of an upsampled voice out of the top octave. */
std::vector<float> resample(const std::vector<float>& in, int rate) {
    if (rate == kCueRate || in.empty() || rate <= 0) {
        return in;
    }
    ma_resampler_config config =
        ma_resampler_config_init(ma_format_f32, 1, static_cast<ma_uint32>(rate),
            static_cast<ma_uint32>(kCueRate), ma_resample_algorithm_linear);
    ma_resampler resampler;
    if (ma_resampler_init(&config, nullptr, &resampler) != MA_SUCCESS) {
        return {};
    }
    /* The filter's delay, flushed with silence and cut from the start. */
    const ma_uint64 delay_in = ma_resampler_get_input_latency(&resampler);
    const ma_uint64 delay_out = ma_resampler_get_output_latency(&resampler);
    std::vector<float> padded(in);
    padded.resize(in.size() + static_cast<size_t>(delay_in) + 1, 0.0f);
    ma_uint64 expected = 0;
    ma_resampler_get_expected_output_frame_count(&resampler, padded.size(), &expected);
    std::vector<float> out(static_cast<size_t>(expected) + 16);
    ma_uint64 frames_in = padded.size();
    ma_uint64 frames_out = out.size();
    ma_resampler_process_pcm_frames(&resampler, padded.data(), &frames_in, out.data(), &frames_out);
    ma_resampler_uninit(&resampler, nullptr);
    out.resize(static_cast<size_t>(frames_out));
    const size_t skip = std::min(out.size(), static_cast<size_t>(delay_out));
    out.erase(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(skip));
    const size_t wanted = in.size() * static_cast<size_t>(kCueRate) / static_cast<size_t>(rate);
    if (out.size() > wanted) {
        out.resize(wanted);
    }
    return out;
}

}  // namespace

std::vector<int> sort_voices(std::vector<VoiceInfo>& voices) {
    std::vector<int> order(voices.size());
    for (size_t i = 0; i < order.size(); i++) {
        order[i] = static_cast<int>(i);
    }
    std::stable_sort(order.begin(), order.end(), [&voices](int a, int b) {
        const VoiceInfo& x = voices[static_cast<size_t>(a)];
        const VoiceInfo& y = voices[static_cast<size_t>(b)];
        return x.language != y.language ? x.language < y.language : x.name < y.name;
    });
    std::vector<VoiceInfo> sorted;
    for (int i : order) {
        sorted.push_back(voices[static_cast<size_t>(i)]);
    }
    voices = std::move(sorted);
    return order;
}

int match_voice(const std::vector<VoiceInfo>& voices, std::string_view wanted) {
    if (wanted.empty()) {
        return -1;
    }
    for (size_t i = 0; i < voices.size(); i++) {
        if (same_text(voices[i].name, wanted)) {
            return static_cast<int>(i);
        }
    }
    for (size_t i = 0; i < voices.size(); i++) {
        if (holds(voices[i].name, wanted)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int find_voice(const std::vector<VoiceInfo>& voices, std::string_view wanted, int default_voice) {
    if (voices.empty()) {
        return -1;
    }
    const int found = match_voice(voices, wanted);
    if (found >= 0) {
        return found;
    }
    return default_voice >= 0 && default_voice < static_cast<int>(voices.size()) ? default_voice :
                                                                                   0;
}

Sound even_out(const RenderedLine& line) {
    Sound sound;
    sound.samples = resample(downmix(line), line.rate);
    float peak = 0.0f;
    for (float s : sound.samples) {
        peak = std::max(peak, std::fabs(s));
    }
    if (peak < kSilentPeak) {
        sound.samples.clear();
        return sound;
    }
    const float gain = kVoicePeak / peak;
    for (float& s : sound.samples) {
        s *= gain;
    }
    return sound;
}

Voices::Voices(
    Speech& speech, CueEngine& engine, std::unique_ptr<VoiceBridge> bridge, bool log, bool preview)
    : m_speech(speech), m_engine(engine), m_log(log), m_preview(preview),
      m_bridge(std::move(bridge)) {
    const char* names[kVoiceCount] = {"your voice", "opponents' voice"};
    for (int i = 0; i < kVoiceCount; i++) {
        VoiceSpec spec;
        spec.name = names[i];
        spec.mode = VoiceMode::once;
        m_voices[static_cast<size_t>(i)].engine_voice = m_engine.add_voice(spec);
    }
    slot(Voice::yours).wanted = "Zira";
    slot(Voice::opponents).wanted = "Mark";
    m_speech.attach_voices(this);
}

Voices::~Voices() {
    m_speech.attach_voices(nullptr);
    for (VoiceSlot& v : m_voices) {
        m_engine.stop(v.engine_voice);
    }
    if (m_worker.joinable()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_quit = true;
        }
        m_wake.notify_all();
        m_worker.join();
    }
}

void Voices::start() {
    if (m_state != VoicesState::stopped) {
        return;
    }
    m_state = VoicesState::starting;
    m_worker = std::thread([this] { work(); });
}

void Voices::log(const char* fmt, ...) const {
    if (!m_log) {
        return;
    }
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    pc_log_line("[a11y] %s", line);
}

void Voices::drop(int voice, const Line& line, const char* why) const {
    log("voice %s dropped (%s): \"%.*s\"", voice_name(static_cast<Voice>(voice)), why,
        text_length(line.text), line.text.data());
}

void Voices::say(Voice voice, Subject subject, std::string_view text) {
    const int index = static_cast<int>(voice);
    VoiceSlot& v = slot(voice);
    Line line;
    line.id = m_next_id++;
    line.subject = subject;
    line.text = std::string(text);
    line.said = Clock::now();
    if (m_state == VoicesState::unavailable) {
        /* Neither the subject nor the voice counts there. */
        m_speech.announce(text, Mode::queue);
        return;
    }
    auto same = std::find_if(v.waiting.begin(), v.waiting.end(),
        [subject](const Line& waiting) { return waiting.subject == subject; });
    if (same != v.waiting.end()) {
        /* In its place, or still cutting the line playing if it was. */
        drop(index, *same, "replaced");
        line.cuts = same->cuts;
        *same = line;
    } else if (v.playing.on && v.playing.subject == subject) {
        line.cuts = true;
        v.waiting.push_front(line);
    } else {
        v.waiting.push_back(line);
    }
    ask_render(index, line);
}

void Voices::ask_render(int voice, const Line& line) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const VoiceSlot& v = m_voices[static_cast<size_t>(voice)];
        m_jobs.push_back(Job{voice, line.id, line.text, v.wanted, v.rate});
    }
    m_wake.notify_one();
}

void Voices::drop_waiting(const char* why) {
    for (int i = 0; i < kVoiceCount; i++) {
        VoiceSlot& v = m_voices[static_cast<size_t>(i)];
        for (const Line& line : v.waiting) {
            drop(i, line, why);
        }
        v.waiting.clear();
    }
}

void Voices::frame(Clock::time_point now, bool port_menu_open) {
    take_news();
    if (port_menu_open && !m_port_menu_open) {
        drop_waiting("port menu");
    }
    m_port_menu_open = port_menu_open;
    for (int i = 0; i < kVoiceCount; i++) {
        advance(i);
    }
    preview_frame(now);
}

void Voices::take_news() {
    std::vector<Rendered> rendered;
    bool open_news = false;
    std::string error;
    std::vector<VoiceInfo> listed;
    int default_index = -1;
    double test_ms = 0.0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        rendered.swap(m_rendered);
        if (m_open_done && !m_open_seen) {
            m_open_seen = true;
            open_news = true;
            error = m_open_error;
            listed = m_listed;
            default_index = m_listed_default;
            test_ms = m_test_ms;
        }
    }
    if (open_news) {
        opened(error, std::move(listed), default_index, test_ms);
    }
    for (Rendered& r : rendered) {
        VoiceSlot& v = m_voices[static_cast<size_t>(r.voice)];
        auto line = std::find_if(
            v.waiting.begin(), v.waiting.end(), [&r](const Line& l) { return l.id == r.id; });
        if (line == v.waiting.end()) {
            continue; /* replaced or dropped since it was asked for */
        }
        if (!r.error.empty()) {
            const std::string why = "render failed: " + r.error;
            drop(r.voice, *line, why.c_str());
            v.waiting.erase(line);
            continue;
        }
        line->rendered = true;
        line->samples = std::move(r.samples);
        line->render_ms = r.ms;
    }
}

void Voices::opened(
    const std::string& error, std::vector<VoiceInfo> listed, int default_index, double test_ms) {
    if (!error.empty()) {
        m_state = VoicesState::unavailable;
        /* The screen reader is told by the Voice rows (voice_settings.hpp),
         * when it will be heard. */
        log("voices unavailable: %s", error.c_str());
        /* The lines waiting, in the order they were said, whichever voice. */
        std::vector<Line> waiting;
        for (VoiceSlot& v : m_voices) {
            waiting.insert(waiting.end(), std::make_move_iterator(v.waiting.begin()),
                std::make_move_iterator(v.waiting.end()));
            v.waiting.clear();
        }
        std::sort(waiting.begin(), waiting.end(),
            [](const Line& a, const Line& b) { return a.id < b.id; });
        for (const Line& line : waiting) {
            m_speech.announce(line.text, Mode::queue);
        }
        return;
    }
    m_state = VoicesState::ready;
    m_installed = std::move(listed);
    m_default_index = default_index;
    if (default_index >= 0 && default_index < static_cast<int>(m_installed.size())) {
        m_default = m_installed[static_cast<size_t>(default_index)].name;
    }
    log("voices: OneCore, %d installed, default %s, test line rendered in %.0f ms",
        static_cast<int>(m_installed.size()), m_default.empty() ? "unknown" : m_default.c_str(),
        test_ms);
    for (int i = 0; i < kVoiceCount; i++) {
        const Voice voice = static_cast<Voice>(i);
        log("voice %s speaks with %s", voice_name(voice), voice_in_use(voice).c_str());
    }
}

void Voices::advance(int voice) {
    VoiceSlot& v = m_voices[static_cast<size_t>(voice)];
    const bool sounding = m_engine.playing(v.engine_voice);
    if (!sounding) {
        v.playing = Playing{};
    }
    while (!v.waiting.empty()) {
        Line& next = v.waiting.front();
        if (!next.rendered || (sounding && !next.cuts)) {
            return;
        }
        if (next.samples.empty()) {
            drop(voice, next, "no sound rendered");
            v.waiting.pop_front();
            continue;
        }
        if (sounding && v.playing.on) {
            log("voice %s cut: \"%.*s\"", voice_name(static_cast<Voice>(voice)),
                text_length(v.playing.text), v.playing.text.data());
        }
        Sound sound;
        sound.samples = std::move(next.samples);
        m_engine.play(v.engine_voice, std::move(sound));
        log("voice %s plays: \"%.*s\" (rendered in %.0f ms, %.0f ms after it was said)",
            voice_name(static_cast<Voice>(voice)), text_length(next.text), next.text.data(),
            next.render_ms, ms_between(next.said, Clock::now()));
        v.playing = Playing{true, next.subject, std::move(next.text)};
        v.waiting.pop_front();
        return;
    }
}

void Voices::preview_frame(Clock::time_point now) {
    if (!m_preview || m_state != VoicesState::ready) {
        return;
    }
    switch (m_preview_step) {
    case 0:
        m_speech.announce_voice(Voice::yours, kPreviewSubject, "Your voice");
        m_preview_step = 1;
        break;
    case 1:
        if (!speaking(Voice::yours)) {
            m_preview_at = now + kPreviewPause;
            m_preview_step = 2;
        }
        break;
    case 2:
        if (now >= m_preview_at) {
            m_speech.announce_voice(Voice::opponents, kPreviewSubject, "Opponents' voice");
            m_preview_step = 3;
        }
        break;
    default:
        break;
    }
}

void Voices::set_voice(Voice voice, std::string_view wanted) {
    slot(voice).wanted = std::string(wanted);
}

std::string Voices::voice_in_use(Voice voice) const {
    /* The worker finds it the same way, in the same sorted list. */
    const int index = find_voice(m_installed, slot(voice).wanted, m_default_index);
    return index < 0 ? std::string() : m_installed[static_cast<size_t>(index)].name;
}

void Voices::set_rate(Voice voice, float rate) {
    slot(voice).rate = std::clamp(std::isfinite(rate) ? rate : 0.5f, 0.0f, 1.0f);
}

void Voices::set_volume(Voice voice, float volume) {
    VoiceSlot& v = slot(voice);
    VoiceTargets targets;
    targets.gain = volume;
    m_engine.set_targets(v.engine_voice, targets);
}

bool Voices::speaking(Voice voice) const {
    const VoiceSlot& v = slot(voice);
    return !v.waiting.empty() || m_engine.playing(v.engine_voice);
}

void Voices::wait_for_worker() {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_worker.joinable()) {
        return;
    }
    m_idle.wait(lock, [this] { return m_open_done && m_jobs.empty() && !m_busy; });
}

/* The worker thread. Prism is started here, for the voices alone
 * (screen_reader_bridge.hpp). */
void Voices::work() {
    std::string error = m_bridge->open(kVoiceCount);
    double test_ms = 0.0;
    /* Sorted as the menus show them, each with its index in the bridge's
     * order, so the worker and the game thread find a voice alike. */
    std::vector<int> bridge_index;
    if (error.empty()) {
        m_worker_voices = m_bridge->voices();
        const int bridge_default = m_bridge->default_voice();
        bridge_index = sort_voices(m_worker_voices);
        for (size_t i = 0; i < bridge_index.size(); i++) {
            if (bridge_index[i] == bridge_default) {
                m_worker_default = static_cast<int>(i);
            }
        }
        if (m_worker_voices.empty()) {
            error = "no voices installed";
        } else {
            RenderedLine line;
            const Clock::time_point start = Clock::now();
            const std::string failed = m_bridge->render(0, kTestLine, line);
            test_ms = ms_between(start, Clock::now());
            if (!failed.empty()) {
                error = "the test line failed: " + failed;
            } else if (line.samples.empty()) {
                error = "the test line rendered nothing";
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open_done = true;
        m_open_error = error;
        m_listed = m_worker_voices;
        m_listed_default = m_worker_default;
        m_test_ms = test_ms;
    }
    m_idle.notify_all();
    /* Each instance starts at the default voice and the normal rate. */
    std::array<int, kVoiceCount> voice_set;
    voice_set.fill(m_worker_default);
    std::array<float, kVoiceCount> rate_set;
    rate_set.fill(0.5f);
    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock, [this] { return m_quit || !m_jobs.empty(); });
            if (m_quit) {
                break;
            }
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
            m_busy = true;
        }
        Rendered rendered;
        rendered.voice = job.voice;
        rendered.id = job.id;
        if (!error.empty()) {
            rendered.error = error;
        } else {
            const size_t instance = static_cast<size_t>(job.voice);
            const int index = find_voice(m_worker_voices, job.wanted, m_worker_default);
            if (index >= 0 && index != voice_set[instance] &&
                m_bridge->set_voice(job.voice, bridge_index[static_cast<size_t>(index)]).empty())
            {
                voice_set[instance] = index;
            }
            if (job.rate != rate_set[instance] && m_bridge->set_rate(job.voice, job.rate).empty()) {
                rate_set[instance] = job.rate;
            }
            RenderedLine line;
            const Clock::time_point start = Clock::now();
            rendered.error = m_bridge->render(job.voice, job.text, line);
            if (rendered.error.empty()) {
                rendered.samples = even_out(line).samples;
            }
            rendered.ms = ms_between(start, Clock::now());
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_rendered.push_back(std::move(rendered));
            m_busy = false;
        }
        m_idle.notify_all();
    }
    m_bridge->close();
}

}  // namespace a11y
