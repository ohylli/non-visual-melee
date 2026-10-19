/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cue_system.hpp"
#include "cue_engine.hpp"
#include "cue_output.hpp"
#include "cue_settings.hpp"
#include "game_access.h"
#include "match_cues.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include "value_file.hpp"
#include "value_model.hpp"
#include <SDL3/SDL.h>
#include <exception>
#include <string>
#include <vector>

namespace a11y {
namespace {

/* MELEE_A11Y_CUE_SETTINGS, or a11y-cues.ini in the base port's settings
 * folder (pc_user_dir, which MELEE_USER_DIR moves); empty for no file: no
 * such folder, or the variable set but empty. UTF-8. */
std::string cue_settings_path(const Config& config) {
    if (config.cue_settings.has_value()) {
        if (config.cue_settings->empty() && config.log) {
            pc_log_line("[a11y] cue settings: MELEE_A11Y_CUE_SETTINGS is empty; using the "
                        "defaults, no file");
        }
        return *config.cue_settings;
    }
    char* folder = pc_user_dir();
    if (folder == nullptr) {
        if (config.log) {
            pc_log_line(
                "[a11y] cue settings: no settings folder (%s); using the defaults", SDL_GetError());
        }
        return "";
    }
    std::string path = std::string(folder) + "a11y-cues.ini";
    SDL_free(folder);
    return path;
}

}  // namespace

CueSystem::CueSystem(Speech& speech) : m_speech(speech) {}

CueSystem::~CueSystem() = default;

std::unique_ptr<CueSystem> CueSystem::open(
    const Config& config, Speech& speech, float master_volume) {
    if (!config.cues) {
        return nullptr;
    }
    std::unique_ptr<CueSystem> system(new CueSystem(speech));
    CueEngineOptions options;
    options.log = config.log;
    options.dump = config.cue_dump;
    options.trace = config.cue_trace;
    system->m_engine = std::make_unique<CueEngine>(options);
    system->m_engine->set_master_volume(master_volume);
    std::string why;
    system->m_output = CueOutput::open(*system->m_engine, config.log, why);
    if (system->m_output == nullptr) {
        if (config.log) {
            pc_log_line("[a11y] cue engine could not start: %s", why.c_str());
        }
        speech.announce("Audio cues could not start.", Mode::queue);
        return nullptr;
    }
    system->open_settings(config);
    return system;
}

/* The cue settings, once every cue has registered its values: read from the
 * file and fed to the engine. */
void CueSystem::open_settings(const Config& config) {
    m_model = std::make_unique<ValueModel>();
    m_cue_settings = std::make_unique<CueSettings>(*m_model);
    /* Each cue registers its values here, before the file is read, so a
     * value the file lacks is added to it. */
    m_match_cues = std::make_unique<MatchCues>(*m_cue_settings, *m_engine);
    std::string path = cue_settings_path(config);
    if (!path.empty()) {
        /* The one place a path from outside meets std::filesystem, which
         * throws on text that is not UTF-8 (getenv gives the ANSI code page
         * on Windows); nothing may escape into the game's C. */
        try {
            m_file =
                std::make_unique<ValueFile>(*m_model, m_speech, "Cue settings", path, config.log);
            m_file->load(Clock::now());
        } catch (const std::exception& error) {
            m_file.reset();
            if (config.log) {
                pc_log_line("[a11y] cue settings: cannot use %s (%s); using the defaults",
                    path.c_str(), error.what());
            }
        }
    }
    m_cue_settings->apply(*m_engine);
}

/* About twice a second at most, it looks whether the cue settings file
 * changed and applies what did. Called from scene entry and, in a match,
 * each tick. */
void CueSystem::poll_settings() {
    if (m_file == nullptr) {
        return;
    }
    std::vector<const NamedValue*> changed = m_file->poll(Clock::now());
    if (!changed.empty()) {
        m_cue_settings->apply(*m_engine, changed);
    }
}

void CueSystem::scene_entered() {
    m_match_running = false;
    m_engine->stop_all();
    if (m_file != nullptr) {
        m_problems_at_scene = m_file->problems_spoken();
    }
    /* A screen that speaks its opening on its first frame still cuts the
     * error off. */
    poll_settings();
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
        if (!m_match_running && m_file != nullptr && !m_file->problem().empty() &&
            m_file->problems_spoken() == m_problems_at_scene)
        {
            /* Said once already, where an earlier scene's own speech may
             * have cut it off; a match is where the player tunes and
             * listens. Said in this scene, as the match scene was entered,
             * it was heard. */
            m_speech.announce(m_file->problem(), Mode::queue);
        }
        poll_settings();
    }
    m_match_running = match.running;
    m_match_cues->tick(match);
}

}  // namespace a11y
