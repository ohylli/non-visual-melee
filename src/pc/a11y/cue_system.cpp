/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cue_system.hpp"
#include "accessibility_settings.hpp"
#include "cue_engine.hpp"
#include "cue_output.hpp"
#include "cue_settings.hpp"
#include "cue_sounds.hpp"
#include "game_access.h"
#include "match_cues.hpp"
#include "pc/pc.h"
#include "sfx_position.hpp"
#include "sfx_script_pan.hpp"
#include "stage_sounds.hpp"
#include "speech.hpp"
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace a11y {
namespace {

/* The pause between the previews of MELEE_A11Y_CUE_PREVIEW's list, so the
 * dump shows each apart. */
constexpr std::chrono::milliseconds kListPause{700};

/* "your_position,gap_warning": the ids, spaces and empty items dropped. */
std::vector<std::string> split_ids(const std::string& text) {
    std::vector<std::string> ids;
    std::string id;
    for (char c : text + ",") {
        if (c == ',') {
            if (!id.empty()) {
                ids.push_back(id);
            }
            id.clear();
        } else if (c != ' ') {
            id += c;
        }
    }
    return ids;
}

}  // namespace

/* Positional sound effects' part of the accessibility settings, in Cue
 * settings with the cues they sit beside. */
struct SfxValues {
    Switch on;
    Switch relative;
    Number far_level; /* % */
    Number side_distance;
    Number near_distance;
    Number far_distance;
    /* Stage sounds, a group of its own beside it. */
    Switch stage_sounds;

    static Switch add_stage_sounds(ValueGroup group) {
        return group.add_switch(SwitchSpec{"on", "Stage sounds",
            "The sounds a stage makes of its own, such as Corneria's Arwings, Mute City's cars or "
            "Pokemon Stadium's crowd; off mutes them, so the fighters are easier to hear, though "
            "a stage hazard then comes without its sound. Stage echo is the Reverb setting with "
            "the volumes instead.",
            true, true});
    }

    /* Both groups made first, in the order the menus and the file list them. */
    static SfxValues add(ValueGroup group, ValueGroup stage_group) {
        const SfxMapping defaults;
        return SfxValues{
            group.add_switch(SwitchSpec{"on", "Positional sound effects",
                "The game's own sounds of each fighter, its attacks, jumps and landings, come "
                "from where that fighter is and grow quieter the further away they are; off, "
                "every fighter's sounds play in the middle, as the game made them.",
                true, true}),
            group.add_switch(SwitchSpec{"relative", "Sound effects relative to you",
                "On, a fighter's sounds come from their side of you, your own from the middle; "
                "off, from where they are across the stage, as the position cues pan.",
                defaults.relative, true}),
            group.add_number(NumberSpec{"far_level", "Far sound effects level",
                "How loud a fighter's sounds are at the far distance and beyond; close to you "
                "they play at full volume.",
                "%", std::round(defaults.far_level * 100.0f), 0.0f, 100.0f, 5.0f, true}),
            group.add_number(NumberSpec{"side_distance", "Side distance",
                "Relative to you: how far to your side a fighter's sounds come fully from that "
                "side.",
                "units", defaults.side_distance, 10.0f, 300.0f, 5.0f}),
            group.add_number(NumberSpec{"near_distance", "Near distance",
                "Within this distance of you a fighter's sounds play at full volume.", "units",
                defaults.near_distance, 0.0f, 100.0f, 1.0f}),
            group.add_number(NumberSpec{"far_distance", "Far distance",
                "From this distance a fighter's sounds play at the far level. Final "
                "Destination's main floor is about 171 units wide.",
                "units", defaults.far_distance, 10.0f, 400.0f, 5.0f}),
            add_stage_sounds(stage_group),
        };
    }

    SfxMapping mapping(bool pan_to_camera) const {
        SfxMapping m;
        m.relative = relative.on();
        m.pan_to_camera = pan_to_camera;
        m.side_distance = side_distance.value();
        m.near_distance = near_distance.value();
        m.far_distance = far_distance.value();
        m.far_level = far_level.value() / 100.0f;
        return m;
    }
};

CueSystem::CueSystem(Speech& speech, AccessibilitySettings& settings)
    : m_speech(speech), m_settings(settings) {}

CueSystem::~CueSystem() = default;

std::unique_ptr<CueSystem> CueSystem::open(
    const Config& config, Speech& speech, AccessibilitySettings& settings, float master_volume) {
    std::unique_ptr<CueSystem> system(new CueSystem(speech, settings));
    system->m_log = config.log;
    system->m_preview_list = split_ids(config.cue_preview);
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
    MatchCueSounds sounds;
    sounds.item_appearance = load_sound(cue_sound_folder() + "item_appearance.wav");
    sounds.item_position = load_sound(cue_sound_folder() + "item_position.wav");
    system->m_match_cues =
        std::make_unique<MatchCues>(*system->m_cue_settings, *system->m_engine, std::move(sounds));
    ValueGroup sfx_group =
        system->m_cue_settings->add_shared("positional_sfx", "Positional sound effects");
    ValueGroup stage_group = system->m_cue_settings->add_shared("stage_sounds", "Stage sounds");
    system->m_sfx_values = std::make_unique<SfxValues>(SfxValues::add(sfx_group, stage_group));
    system->m_stage_sounds = std::make_unique<StageSounds>(
        [](int voice, std::uint8_t volume) { return a11y_game_sfx_set_volume(voice, volume); });
    system->m_sfx = std::make_unique<SfxPositions>(SfxPositions::Engine{
        [](int voice, std::uint8_t pan) { return a11y_game_sfx_set_pan(voice, pan); },
        [](int voice, std::uint8_t pan) { sfx_remember_pan(voice, pan); },
    });
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
    m_match_cues->scene_entered();
    m_sfx->forget();
    /* Before the hook polls the settings file, so an error it finds now
     * counts as said in this scene. */
    m_problems_at_scene = m_settings.problems_spoken();
}

void CueSystem::set_allowed(bool allowed) {
    m_allowed = allowed;
    m_cue_settings->set_allowed(allowed);
    m_engine->set_mix_level(m_cue_settings->mix_level());
}

void CueSystem::set_master_volume(float volume) {
    m_engine->set_master_volume(volume);
}

void CueSystem::silence_for_port_menu() {
    m_match_cues->silence();
}

bool CueSystem::preview(std::string_view id, Clock::time_point now) {
    const std::string name(id);
    const bool known = MatchCues::has_preview(id);
    if (m_log) {
        /* Before the voices' start lines. */
        pc_log_line(
            known ? "[a11y] cue preview: %s" : "[a11y] cue preview: no cue %s", name.c_str());
    }
    return known && m_match_cues->preview(id, now);
}

void CueSystem::stop_preview(Clock::time_point now) {
    m_match_cues->stop_preview(now);
}

bool CueSystem::previewing() const {
    return m_match_cues->previewing();
}

void CueSystem::preview_frame(Clock::time_point now) {
    m_match_cues->preview_frame(now);
    if (m_preview_next >= m_preview_list.size()) {
        return;
    }
    if (m_list_previewing) {
        if (m_match_cues->previewing()) {
            return;
        }
        m_list_previewing = false;
        m_preview_list_at = now + kListPause;
    }
    if (now < m_preview_list_at) {
        return;
    }
    m_list_previewing = preview(m_preview_list[m_preview_next++], now);
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
    A11ySfxSources sources;
    a11y_game_sfx_sources(&sources);
    m_sfx->tick(m_allowed && m_sfx_values->on.on(), match, sources,
        m_sfx_values->mapping(m_match_cues->pan_to_camera()));
    m_stage_sounds->tick(mute_stage_sounds());
}

bool CueSystem::mute_stage_sounds() const {
    return m_allowed && !m_sfx_values->stage_sounds.on();
}

void CueSystem::sfx_started(int voice, int sound, int track, std::uint8_t* volume,
    std::uint8_t* pan, std::uint8_t* script_pan) {
    if (m_stage_sounds->started(mute_stage_sounds(), voice, a11y_game_sfx_bank(sound), volume)) {
        return;
    }
    m_sfx->started(voice, track, volume, pan, script_pan);
}

int CueSystem::sound_object_pan(int slot, bool sub, int pan) {
    const std::optional<SfxPlace> at = m_sfx->place(SfxSource{slot, sub ? 1 : 0});
    /* The engine's 0 to 0xFF, centre 0x80, as the game's 0 to 127. */
    return at.has_value() ? at->pan / 2 : pan;
}

int CueSystem::sound_object_volume(int slot, bool sub, int volume) {
    const std::optional<SfxPlace> at = m_sfx->place(SfxSource{slot, sub ? 1 : 0});
    if (!at.has_value()) {
        return volume;
    }
    return static_cast<int>(std::lround(static_cast<float>(volume) * at->gain));
}

void CueSystem::sfx_track_hint(int track) {
    m_sfx->track_hint(track);
}

void CueSystem::fighter_sfx(int slot, bool sub) {
    m_sfx->fighter_next(SfxSource{slot, sub ? 1 : 0});
}

std::span<const A11yMatchItem> CueSystem::items_appeared() const {
    return m_match_cues->items_appeared();
}

}  // namespace a11y
