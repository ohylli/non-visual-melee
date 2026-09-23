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

const char* mode_name(Mode mode) {
    return mode == Mode::interrupt ? "interrupt" : "queue";
}

int text_length(std::string_view text) {
    return static_cast<int>(text.size());
}

}  // namespace

Config config_from_environment() {
    Config config;
    config.enabled = env_switch("MELEE_A11Y");
    config.log = env_switch("MELEE_A11Y_LOG");
    return config;
}

Speech::Speech(Config config, std::unique_ptr<ScreenReaderBridge> bridge)
    : m_config(config), m_bridge(std::move(bridge)) {}

void Speech::init() {
    m_thread = std::this_thread::get_id();
    if (!m_config.enabled) {
        log("speech off (MELEE_A11Y=0)");
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

void Speech::announce(std::string_view text, Mode mode) {
    /* Checked first, so another thread never writes m_last or reaches the
     * bridge, neither of which is thread-safe. The log line is the one thing
     * it still touches: a bug report worth the small risk of a garbled line. */
    if (m_thread == std::thread::id()) {
        log("speech called before init, dropped: \"%.*s\"", text_length(text), text.data());
        return;
    }
    if (!initialized_on_this_thread()) {
        log("speech called off the game thread, dropped: \"%.*s\"", text_length(text), text.data());
        return;
    }
    m_last = Announcement{std::string(text), mode};
    if (!m_config.enabled) {
        log("speak %s (off): \"%.*s\"", mode_name(mode), text_length(text), text.data());
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
