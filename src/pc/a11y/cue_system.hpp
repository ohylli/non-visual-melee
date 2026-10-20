/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cue system (primer docs/a11y/cues.md): the cue engine and its output,
 * the cues' part of the accessibility settings and the cues of a match,
 * opened together as the game starts and closed together at shutdown. The
 * hooks forward to it. Game thread only. */
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace a11y {

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
     * looked at: every cue stops, since cues belong to the screen they play
     * on. */
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
    /* Each frame boundary while the port menu is open: the cues stand
     * silent. The game stands still under the menu, so no tick runs to stop
     * them, and the voices would sound on as the last tick left them. The
     * first tick after it closes starts them again. */
    void silence_for_port_menu();

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
};

}  // namespace a11y
