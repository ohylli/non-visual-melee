/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cue system (primer docs/a11y/cues.md): the cue engine and its output,
 * the cues' part of the accessibility settings and the cues of a match,
 * opened together as the game starts and closed together at shutdown. The
 * hooks forward to it. Game thread only. */
#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

using Clock = std::chrono::steady_clock;

struct Config;
class AccessibilitySettings;
class CueEngine;
class CueOutput;
class CueSettings;
class MatchCues;
struct NamedValue;
class Speech;

class CueSystem {
public:
    /* Opens the engine, not on first use: opening the stream takes part of a
     * frame (ADR-0005). Then every cue registers its values in the
     * accessibility settings, which are read after it, and the engine gets
     * their levels at the defaults. Opened whatever the switches say, which
     * mute it rather than close it, so they are live. When the output could
     * not open, which is logged, the cues run unheard and sounding() says
     * so. master_volume is the launcher's, kept from before. */
    static std::unique_ptr<CueSystem> open(
        const Config& config, Speech& speech, AccessibilitySettings& settings, float master_volume);
    ~CueSystem();
    CueSystem(const CueSystem&) = delete;
    CueSystem& operator=(const CueSystem&) = delete;

    /* After the scene's announcement and before the settings file is
     * looked at: the match's cues stop, since they belong to the screen
     * they play on. A preview plays on. */
    void scene_entered();
    /* Values of the accessibility settings changed: the levels they make up
     * go to the engine. */
    void settings_changed(const std::vector<const NamedValue*>& changed);
    /* The output opened: false when the cues run unheard. */
    bool sounding() const { return m_output != nullptr; }
    /* Whether Accessibility lets the cues sound (Switches::cues_allowed). */
    void set_allowed(bool allowed);
    /* The launcher's master volume with mute applied. */
    void set_master_volume(float volume);
    /* After every simulated tick, not while rollback re-runs ticks: reads
     * the match snapshot (a11y_game_match_state, with what the cues ask of
     * it) and hands it to the cues. */
    void tick(int local_port, bool online, uint64_t held_mask);
    /* Each frame boundary while the port menu is open: the match's cues
     * stand silent. The game stands still under the menu, so no tick runs to
     * stop them, and the voices would sound on as the last tick left them.
     * The first tick after it closes starts them again. A preview, started
     * from the menu, plays on. */
    void silence_for_port_menu();

    /* A cue's preview (MatchCues::preview, docs/a11y/cues.md, "Preview"):
     * about two seconds of the cue with this id, the id add_cue took
     * ("gap_warning", a Preview row's action group), at a fixed, typical
     * moment of a match, whatever the switches say and at every volume.
     * Again restarts it; another cue's stops the one playing. Logged; false,
     * with a log line, for an id with no preview. Game thread, outside the
     * simulation: from the launcher's and the port menu's frames. */
    bool preview(std::string_view id, Clock::time_point now);
    /* The preview stops, with the engine's ramp: as its row loses focus or
     * its page closes. Nothing when none plays. */
    void stop_preview(Clock::time_point now);
    bool previewing() const;
    /* Each frame, from the launcher's frame hook and from the port menu's,
     * which runs at every frame boundary, open or not: the preview follows
     * its moment and ends. Also plays MELEE_A11Y_CUE_PREVIEW's list. */
    void preview_frame(Clock::time_point now);

private:
    CueSystem(Speech& speech, AccessibilitySettings& settings);
    void poll_settings();

    Speech& m_speech;
    AccessibilitySettings& m_settings;
    /* Destroyed in reverse: the output first, since its callback renders the
     * engine, and the engine last. */
    std::unique_ptr<CueEngine> m_engine;
    std::unique_ptr<CueSettings> m_cue_settings;
    std::unique_ptr<MatchCues> m_match_cues;
    std::unique_ptr<CueOutput> m_output;
    /* The last tick read had a match running, since the scene was entered. */
    bool m_match_running = false;
    /* The settings file's count of spoken errors as the scene was entered:
     * one spoken since then, in the scene's own quiet, is not repeated. */
    int m_problems_at_scene = 0;
    bool m_log = true;
    /* MELEE_A11Y_CUE_PREVIEW, a developer switch: the cues' ids whose
     * previews play in turn from the first frame, a pause apart, for a
     * check through the cue dump. */
    std::vector<std::string> m_preview_list;
    std::size_t m_preview_next = 0;
    Clock::time_point m_preview_list_at;
    bool m_list_previewing = false;
};

}  // namespace a11y
