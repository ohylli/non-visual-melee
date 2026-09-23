/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "screen_reader_bridge.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace a11y {

/* interrupt cuts off whatever is being spoken; queue speaks after it. Queue is
 * for separate happenings, never for the parts of one announcement. */
enum class Mode { interrupt, queue };

struct Config {
    bool enabled = true; /* MELEE_A11Y: the accessibility switch */
    bool log = true;     /* MELEE_A11Y_LOG: the speech log, a developer switch */
};

/* The only place the configuration is read from; swap its body when fork
 * settings get their own storage. */
Config config_from_environment();

struct Announcement {
    std::string text;
    Mode mode = Mode::interrupt;
};

/* Speech: takes announcements from every feature, writes the speech log and
 * honours the accessibility switch. Game thread only: it remembers the thread
 * init ran on and drops calls from any other. */
class Speech {
public:
    Speech(Config config, std::unique_ptr<ScreenReaderBridge> bridge);

    void init();
    void shutdown();
    /* Speaks exactly what it is given, every time: no duplicate suppression.
     * text is UTF-8. */
    void announce(std::string_view text, Mode mode);
    /* The most recent announcement, spoken or not. For a later "repeat last"
     * key. */
    const Announcement& last() const { return m_last; }
    bool initialized_on_this_thread() const;

private:
    /* One speech log line, prefixed "[a11y] "; nothing when the log switch is
     * off. */
    void log(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));

    Config m_config;
    std::unique_ptr<ScreenReaderBridge> m_bridge;
    bool m_bridge_initialized = false;
    std::thread::id m_thread;
    Announcement m_last;
};

}  // namespace a11y
