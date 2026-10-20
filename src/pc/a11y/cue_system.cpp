/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cue_system.hpp"
#include "accessibility_settings.hpp"
#include "cue_engine.hpp"
#include "cue_output.hpp"
#include "cue_settings.hpp"
#include "game_access.h"
#include "match_cues.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <string>
#include <vector>

namespace a11y {

CueSystem::CueSystem(Speech& speech, AccessibilitySettings& settings)
    : m_speech(speech), m_settings(settings) {}

CueSystem::~CueSystem() = default;

std::unique_ptr<CueSystem> CueSystem::open(
    const Config& config, Speech& speech, AccessibilitySettings& settings, float master_volume) {
    std::unique_ptr<CueSystem> system(new CueSystem(speech, settings));
    CueEngineOptions options;
    options.log = config.log;
    options.dump = config.cue_dump;
    options.trace = config.cue_trace;
    system->m_engine = std::make_unique<CueEngine>(options);
    system->m_engine->set_master_volume(master_volume);
    std::string why;
    system->m_output = CueOutput::open(*system->m_engine, config.log, why);
    if (system->m_output == nullptr && config.log) {
        pc_log_line("[a11y] cue engine could not start: %s", why.c_str());
    }
    system->m_cue_settings = std::make_unique<CueSettings>(settings.model());
    /* Each cue registers its values here, before the file is read, so a
     * value the file lacks is added to it. */
    system->m_match_cues = std::make_unique<MatchCues>(*system->m_cue_settings, *system->m_engine);
    system->m_cue_settings->apply(*system->m_engine);
    return system;
}

void CueSystem::settings_changed(const std::vector<const NamedValue*>& changed) {
    m_cue_settings->apply(*m_engine, changed);
}

/* About twice a second at most, it looks whether the settings file changed;
 * what did reaches settings_changed through the settings' listener. Called
 * each tick of a match; the hooks poll at scene entry and in the menus. */
void CueSystem::poll_settings() {
    m_settings.poll(Clock::now());
}

void CueSystem::scene_entered() {
    m_match_running = false;
    m_engine->stop_all();
    /* Before the hook polls the settings file, so an error it finds now
     * counts as said in this scene. */
    m_problems_at_scene = m_settings.problems_spoken();
}

void CueSystem::set_allowed(bool allowed) {
    m_cue_settings->set_allowed(allowed);
    m_engine->set_mix_level(m_cue_settings->mix_level());
}

void CueSystem::set_master_volume(float volume) {
    m_engine->set_master_volume(volume);
}

void CueSystem::silence_for_port_menu() {
    m_match_cues->silence();
}

void CueSystem::tick(int local_port, bool online, uint64_t held_mask) {
    A11yMatchState match;
    a11y_game_match_state(local_port, online, held_mask, m_match_cues->edge_offset(), &match);
    if (match.running) {
        if (!m_match_running && !m_settings.problem().empty() &&
            m_settings.problems_spoken() == m_problems_at_scene)
        {
            /* Said once already, where an earlier scene's own speech may
             * have cut it off; a match is where the player tunes and
             * listens. Said in this scene, as the match scene was entered,
             * it was heard. */
            m_speech.announce(m_settings.problem(), Mode::queue);
        }
        poll_settings();
    }
    m_match_running = match.running;
    m_match_cues->tick(match);
}

}  // namespace a11y
