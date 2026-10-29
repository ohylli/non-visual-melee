/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "speech.hpp"
#include "pc/pc.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace a11y {
namespace {

/* Unset or anything other than "0" means on. */
bool env_switch(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr || std::strcmp(value, "0") != 0;
}

/* An override: unset or empty for none, "0" off, anything else on. */
std::optional<bool> env_override(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::strcmp(value, "0") != 0;
}

const char* mode_name(Mode mode) {
    return mode == Mode::interrupt ? "interrupt" : "queue";
}

int text_length(std::string_view text) {
    return static_cast<int>(text.size());
}

}  // namespace

Config config_from_environment() {
    Config config;
    config.log = env_switch("MELEE_A11Y_LOG");
    config.screen_reader = env_switch("MELEE_A11Y_SCREEN_READER");
    config.accessibility = env_override("MELEE_A11Y");
    config.steering = env_override("MELEE_A11Y_STEER");
    config.cues = env_override("MELEE_A11Y_CUES");
    const char* dump = std::getenv("MELEE_A11Y_CUE_DUMP");
    config.cue_dump = dump != nullptr ? dump : "";
    const char* trace = std::getenv("MELEE_A11Y_CUE_TRACE");
    config.cue_trace = trace != nullptr ? trace : "";
    const char* preview = std::getenv("MELEE_A11Y_CUE_PREVIEW");
    config.cue_preview = preview != nullptr ? preview : "";
    const char* voice_preview = std::getenv("MELEE_A11Y_VOICE_PREVIEW");
    config.voice_preview = voice_preview != nullptr && voice_preview[0] != '\0' &&
                           std::strcmp(voice_preview, "0") != 0;
    const char* settings_file = std::getenv("MELEE_A11Y_SETTINGS");
    if (settings_file != nullptr) {
        config.settings_file = settings_file;
    }
    return config;
}

const char* voice_name(Voice voice) {
    return voice == Voice::yours ? "yours" : "opponents";
}

bool log_enabled() {
    static const bool enabled = config_from_environment().log;
    return enabled;
}

void LogOnce::operator()(const std::string& line) {
    if (m_logged.insert(line).second && log_enabled()) {
        pc_log_line("%s", line.c_str());
    }
}

Speech::Speech(Config config, std::unique_ptr<ScreenReaderBridge> bridge)
    : m_config(config), m_bridge(std::move(bridge)), m_on(config.speech) {}

void Speech::init() {
    m_thread = std::this_thread::get_id();
    if (m_on) {
        start_bridge();
    }
}

void Speech::set_on(bool on) {
    if (on == m_on) {
        return;
    }
    m_on = on;
    if (on && m_thread != std::thread::id()) {
        start_bridge();
    }
    if (!on && m_voices != nullptr) {
        m_voices->drop_waiting("speech off");
    }
}

void Speech::attach_voices(VoiceLines* voices) {
    m_voices = voices;
    if (m_voices != nullptr && m_bridge_started) {
        m_voices->start();
    }
}

void Speech::start_bridge() {
    if (m_bridge_started) {
        return;
    }
    m_bridge_started = true;
    if (m_voices != nullptr) {
        /* Whatever the screen reader switch says: the voices play through
         * the cue engine. */
        m_voices->start();
    }
    if (!m_config.screen_reader) {
        log("screen reader off (MELEE_A11Y_SCREEN_READER=0)");
        return;
    }
    const std::string backend = m_bridge->init();
    m_bridge_initialized = true;
    if (backend.empty()) {
        log("speech backend: none (silent)");
    } else {
        log("speech backend: %s", backend.c_str());
    }
}

void Speech::shutdown() {
    if (m_bridge_initialized) {
        m_bridge->shutdown();
        m_bridge_initialized = false;
    }
    log("speech shutdown");
}

/* Checked first, so another thread never writes m_last or reaches the
 * bridge or the voices, none of which is thread-safe. The log line is the
 * one thing it still touches: a bug report worth the small risk of a garbled
 * line. */
bool Speech::on_game_thread(std::string_view text) const {
    if (m_thread == std::thread::id()) {
        log("speech called before init, dropped: \"%.*s\"", text_length(text), text.data());
        return false;
    }
    if (!initialized_on_this_thread()) {
        log("speech called off the game thread, dropped: \"%.*s\"", text_length(text), text.data());
        return false;
    }
    return true;
}

void Speech::announce_voice(Voice voice, Subject subject, std::string_view text) {
    if (!on_game_thread(text)) {
        return;
    }
    if (!m_on) {
        log("voice %s (off): \"%.*s\"", voice_name(voice), text_length(text), text.data());
        return;
    }
    log("voice %s: \"%.*s\"", voice_name(voice), text_length(text), text.data());
    if (m_voices == nullptr) {
        log("voice %s dropped (no voices): \"%.*s\"", voice_name(voice), text_length(text),
            text.data());
        return;
    }
    m_voices->say(voice, subject, text);
}

void Speech::announce(std::string_view text, Mode mode) {
    if (!on_game_thread(text)) {
        return;
    }
    m_last = Announcement{std::string(text), mode};
    if (!m_on) {
        log("speak %s (off): \"%.*s\"", mode_name(mode), text_length(text), text.data());
        return;
    }
    if (!m_config.screen_reader) {
        log("speak %s (muted): \"%.*s\"", mode_name(mode), text_length(text), text.data());
        return;
    }
    log("speak %s: \"%.*s\"", mode_name(mode), text_length(text), text.data());
    const std::string error = m_bridge->output(text, mode == Mode::interrupt);
    if (!error.empty()) {
        log("speak failed (%s): \"%.*s\"", error.c_str(), text_length(text), text.data());
    }
}

bool Speech::initialized_on_this_thread() const {
    return m_thread == std::this_thread::get_id();
}

/* Formats here because pc_log_line has no va_list variant. Same buffer size as
 * pc_log_line, so this adds no truncation of its own. */
void Speech::log(const char* fmt, ...) const {
    if (!m_config.log) {
        return;
    }
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    pc_log_line("[a11y] %s", line);
}

}  // namespace a11y
