/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cue system (primer docs/a11y/cues.md): the cue engine and its output,
 * the cue settings with the file behind them and the cues of a match, opened
 * together as the game starts and closed together at shutdown. The hooks
 * forward to it. Game thread only. */
#pragma once
#include <cstdint>
#include <memory>

namespace a11y {

struct Config;
class CueEngine;
class CueOutput;
class CueSettings;
class MatchCues;
class Speech;
class ValueFile;
class ValueModel;

class CueSystem {
public:
    /* Opens the engine, not on first use: opening the stream takes part of a
     * frame (ADR-0005). Then every cue registers its values and the cue
     * settings are read. Null while cues are off, and when the output could
     * not open, which is logged and said. master_volume is the launcher's,
     * kept from before. */
    static std::unique_ptr<CueSystem> open(
        const Config& config, Speech& speech, float master_volume);
    ~CueSystem();
    CueSystem(const CueSystem&) = delete;
    CueSystem& operator=(const CueSystem&) = delete;

    /* After the scene's announcement: every cue stops, since cues belong to
     * the screen they play on, and the settings file is looked at, so an
     * error in it is queued behind the announcement. */
    void scene_entered();
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
    explicit CueSystem(Speech& speech);
    void open_settings(const Config& config);
    void poll_settings();

    Speech& m_speech;
    /* Destroyed in reverse: the output first, since its callback renders the
     * engine, and the engine last. The file is null when it has no path. */
    std::unique_ptr<CueEngine> m_engine;
    std::unique_ptr<ValueModel> m_model;
    std::unique_ptr<CueSettings> m_cue_settings;
    std::unique_ptr<ValueFile> m_file;
    std::unique_ptr<MatchCues> m_match_cues;
    std::unique_ptr<CueOutput> m_output;
    /* The last tick read had a match running, since the scene was entered. */
    bool m_match_running = false;
    /* The settings file's count of spoken errors as the scene was entered:
     * one spoken since then, in the scene's own quiet, is not repeated. */
    int m_problems_at_scene = 0;
};

}  // namespace a11y
