/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "screen_reader_bridge.hpp"
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>

namespace a11y {

/* interrupt cuts off whatever is being spoken; queue speaks after it. Queue is
 * for separate happenings, never for the parts of one announcement. */
enum class Mode { interrupt, queue };

/* The fork's own voices beside the screen reader (voices.hpp, primer
 * docs/a11y/speech.md, "Voices"): your voice speaks what happens to you, the
 * opponents' voice what happens to everyone else. */
enum class Voice { yours, opponents };
constexpr int kVoiceCount = 2;

/* "yours" or "opponents", in log lines. */
const char* voice_name(Voice voice);

/* Whom a voice's line is about: a player's slot in a match (0 to 5), or a
 * preview. Within one voice a new line about a subject replaces the one
 * about it already there (voices.hpp). */
using Subject = int;
constexpr Subject kPreviewSubject = -1;

/* The voices as speech sees them: voices.cpp, attached once the cue engine
 * they play through is open. Game thread only. */
class VoiceLines {
public:
    virtual ~VoiceLines() = default;
    /* The first time speech is on: the voices' worker starts, kept for the
     * run. */
    virtual void start() = 0;
    /* A line that speech has logged and let through. */
    virtual void say(Voice voice, Subject subject, std::string_view text) = 0;
    /* The voices stop where they are: each line playing finishes, the
     * lines waiting are dropped, each logged with why. */
    virtual void drop_waiting(const char* why) = 0;
};

struct Config {
    /* Whether speech is on from init(), until set_on says otherwise: the
     * game starts it off and the switches turn it on once the accessibility
     * settings are read (switches.hpp). */
    bool speech = true;
    bool log = true; /* MELEE_A11Y_LOG: the speech log, a developer switch */
    /* MELEE_A11Y_SCREEN_READER=0: accessibility stays on, so every feature
     * acts as it does for a player, but the screen reader is never started
     * and announcements are only logged. For agent runs that check what the
     * switch changes (the launcher's line reading) without talking over the
     * maintainer's screen reader. */
    bool screen_reader = true;
    /* The switches' overrides for the run (switches.hpp): MELEE_A11Y for
     * Accessibility, MELEE_A11Y_STEER for Steering and MELEE_A11Y_CUES for
     * Cues, "0" off and anything else on; unset or empty for none, so the
     * accessibility settings decide. */
    std::optional<bool> accessibility;
    std::optional<bool> steering;
    std::optional<bool> cues;
    /* MELEE_A11Y_CUE_DUMP and MELEE_A11Y_CUE_TRACE: files for the cue
     * engine's mix and its voices' targets; empty for none. */
    std::string cue_dump;
    std::string cue_trace;
    /* MELEE_A11Y_CUE_PREVIEW: cues' ids, comma-separated, whose previews
     * play in turn from the first frame (CueSystem::preview_frame); empty
     * for none. */
    std::string cue_preview;
    /* MELEE_A11Y_VOICE_PREVIEW: set and not "0", each voice says its name
     * once the voices are ready, your voice first (Voices::frame). */
    bool voice_preview = false;
    /* MELEE_A11Y_SETTINGS: the accessibility settings file; unset for
     * a11y.ini in the folder where the base port keeps its settings. Set but
     * empty, as a shell slip leaves it, means no file at all: an agent run
     * that meant a scratch file must not fall back to the player's. */
    std::optional<std::string> settings_file;
};

/* The developer switches and the overrides, from the environment. */
Config config_from_environment();

/* The speech log switch, read once, for features that log lines of their own
 * ("not in the menu names table"). */
bool log_enabled();

/* A feature's own log lines, each written the first time it comes: a gap in
 * the words met every frame is logged once. Under the speech log switch. */
class LogOnce {
public:
    void operator()(const std::string& line);

private:
    std::set<std::string> m_logged;
};

struct Announcement {
    std::string text;
    Mode mode = Mode::interrupt;
};

/* Speech: takes announcements from every feature, writes the speech log and
 * honours its switch, which the switches set (switches.hpp). Game thread
 * only: it remembers the thread init ran on and drops calls from any
 * other. */
class Speech {
public:
    Speech(Config config, std::unique_ptr<ScreenReaderBridge> bridge);

    void init();
    void shutdown();
    /* Speaks exactly what it is given, every time: no duplicate suppression.
     * text is UTF-8. Off, it is only logged, marked (off). */
    void announce(std::string_view text, Mode mode);
    /* A line in one of the voices, about subject, logged naming the voice.
     * Off, it is only logged, marked (off), and never reaches the voices.
     * MELEE_A11Y_SCREEN_READER=0 leaves the voices alone: they play through
     * the cue engine, never through the screen reader. Not kept as last(). */
    void announce_voice(Voice voice, Subject subject, std::string_view text);
    /* The voices, or nullptr to let go of them before they are destroyed.
     * Started now if speech has been on. */
    void attach_voices(VoiceLines* voices);
    /* The most recent announcement, spoken or not. For a later "repeat last"
     * key. */
    const Announcement& last() const { return m_last; }
    /* Whether speech is on: off, nothing is spoken, and features that take
     * keys for speech leave them alone. */
    bool enabled() const { return m_on; }
    /* The switches' decision, live. An announcement made before turning it
     * off has reached the screen reader, which speaks it out: nothing sent
     * after can cut it off. Each voice finishes its line and drops the ones
     * waiting. The screen reader and the voices are started the first time
     * speech is on, so a run that never turns it on never starts them. */
    void set_on(bool on);
    bool initialized_on_this_thread() const;

private:
    /* One speech log line, prefixed "[a11y] "; nothing when the log switch is
     * off. */
    void log(const char* fmt, ...) const __attribute__((format(printf, 2, 3)));
    /* Logs and drops a call from before init or off the game thread. */
    bool on_game_thread(std::string_view text) const;

    /* The screen reader the first time speech is on. */
    void start_bridge();

    Config m_config;
    std::unique_ptr<ScreenReaderBridge> m_bridge;
    bool m_on;
    bool m_bridge_started = false; /* start_bridge has run */
    bool m_bridge_initialized = false;
    VoiceLines* m_voices = nullptr;
    std::thread::id m_thread;
    Announcement m_last;
};

}  // namespace a11y
