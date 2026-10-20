/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "screen_reader_bridge.hpp"
#include <memory>
#include <optional>
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
    /* MELEE_A11Y_SCREEN_READER=0: accessibility stays on, so every feature
     * acts as it does for a player, but the screen reader is never started
     * and announcements are only logged. For agent runs that check what the
     * switch changes (the launcher's line reading) without talking over the
     * maintainer's screen reader. */
    bool screen_reader = true;
    /* Steering with the D-pad: follows the accessibility switch;
     * MELEE_A11Y_STEER=1 turns it on by itself, for agent runs with speech
     * off. */
    bool steer = true;
    /* The cue engine (docs/a11y/cues.md): follows the accessibility switch;
     * MELEE_A11Y_CUES=1 turns it on by itself, for agent runs with speech
     * off, and MELEE_A11Y_CUES=0 off. Off, the engine never opens. */
    bool cues = true;
    /* MELEE_A11Y_CUE_DUMP and MELEE_A11Y_CUE_TRACE: files for the cue
     * engine's mix and its voices' targets; empty for none. */
    std::string cue_dump;
    std::string cue_trace;
    /* MELEE_A11Y_SETTINGS: the accessibility settings file; unset for
     * a11y.ini in the folder where the base port keeps its settings. Set but
     * empty, as a shell slip leaves it, means no file at all: an agent run
     * that meant a scratch file must not fall back to the player's. */
    std::optional<std::string> settings_file;
};

/* The only place the configuration is read from; swap its body when fork
 * settings get their own storage. */
Config config_from_environment();

/* The speech log switch, read once, for features that log lines of their own
 * ("not in the menu names table"). */
bool log_enabled();

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
    /* The accessibility switch: off, nothing is spoken, and features that
     * take keys for speech leave them alone. */
    bool enabled() const { return m_config.enabled; }
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
