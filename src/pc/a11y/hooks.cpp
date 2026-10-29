/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "a11y_hooks.h"
#include "accessibility_settings.hpp"
#include "css_reader.hpp"
#include "cue_system.hpp"
#include "direct_speech.hpp"
#include "event_pipe.hpp"
#include "game_access.h"
#include "game_text.hpp"
#include "launcher_speech.hpp"
#include "lobby_speech.hpp"
#include "match_announcement_settings.hpp"
#include "match_announcer.hpp"
#include "memcard_speech.hpp"
#include "menu_speech.hpp"
#include "menu_text.hpp"
#include "pc/net.h"
#include "pc/paths.h"
#include "pc/pc.h"
#include "pc/region.h"
#include "port_menu_speech.hpp"
#include "results_speech.hpp"
#include "scene_speech.hpp"
#include "settings_page.hpp"
#include "speech.hpp"
#include "sss_reader.hpp"
#include "switches.hpp"
#include "training_speech.hpp"
#include "voice_settings.hpp"
#include "voices.hpp"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <dolphin/pad.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/* Melee's own menu bank, which the port menu plays its sounds from
 * (src/pc/launcher.cpp). */
extern "C" void lbAudioAx_80024030(int);

namespace {

/* Constant-initialised empty, so nothing runs before main; created by
 * pc_a11y_init and destroyed by pc_a11y_shutdown. */
std::unique_ptr<a11y::Speech> s_speech;
std::unique_ptr<a11y::SceneSpeech> s_scene_speech;
std::unique_ptr<a11y::MenuSpeech> s_menu_speech;
std::unique_ptr<a11y::CssReader> s_css;
std::unique_ptr<a11y::SssReader> s_sss;
std::unique_ptr<a11y::LobbySpeech> s_lobby;
std::unique_ptr<a11y::DirectSpeech> s_direct;
std::unique_ptr<a11y::TrainingSpeech> s_training;
std::unique_ptr<a11y::ResultsSpeech> s_results;
std::unique_ptr<a11y::MemcardSpeech> s_memcard;
/* The accessibility settings and their file, a11y.ini. */
std::unique_ptr<a11y::AccessibilitySettings> s_settings;
/* Accessibility, Speech and Steering, and the hierarchy over Cues. */
std::unique_ptr<a11y::Switches> s_switches;
/* The cue engine, the cues' values and the cues of a match, open whatever
 * the switches say. */
std::unique_ptr<a11y::CueSystem> s_cue_system;
/* Your voice and the opponents' voice, playing through the cue system's
 * engine, so destroyed before it. */
std::unique_ptr<a11y::Voices> s_voices;
/* The voices' rows, registered with the switches and attached to the voices
 * once the settings are loaded. */
std::unique_ptr<a11y::VoiceSettings> s_voice_settings;
/* The match announcements and their rows, after the voices' rows. */
std::unique_ptr<a11y::MatchAnnouncer> s_match_announcer;
std::unique_ptr<a11y::MatchAnnouncementSettings> s_match_settings;
/* The launcher's master volume with mute applied, kept from before the
 * engine opens. */
float s_master_volume = 1.0f;
/* The centre text a leaf screen last set, kept across scenes: a screen the
 * menu scene opens on, as Multi-Man Melee after its match, sets it before
 * the scene hook runs. */
int s_center_text = -1;
/* Random Stage refused a toggle since the last menu frame. */
bool s_stage_switch_refused = false;
/* The scene entered last, and character select's pending exit on its last
 * frame, recorded even while rollback re-runs frames: the frame where a
 * back-out shows may be a re-run. They tell the lobby how it was reached. */
int s_scene = -1;
int s_css_exit = A11Y_CSS_STAYING;
/* Character select's pending exit last asked for the rules screens rather
 * than name entry; both then read "away" while open. Recorded even while
 * rollback re-runs frames, like the exit. */
bool s_css_to_rules = false;
/* The menu reader was fed from character select on the last frame read,
 * so the next frame it is not is a new visit. */
bool s_css_menu_fed = false;

/* MELEE_A11Y_SETTINGS, or a11y.ini in the base port's settings folder
 * (pc_get_pref_path, which MELEE_USER_DIR moves); empty for no file: no such
 * folder, or the variable set but empty. UTF-8. */
std::string settings_file_path(const a11y::Config& config) {
    if (config.settings_file.has_value()) {
        if (config.settings_file->empty() && config.log) {
            pc_log_line("[a11y] accessibility settings: MELEE_A11Y_SETTINGS is empty; using the "
                        "defaults, no file");
        }
        return *config.settings_file;
    }
    char* folder = pc_get_pref_path();
    if (folder == nullptr) {
        if (config.log) {
            pc_log_line("[a11y] accessibility settings: no settings folder (%s); using the "
                        "defaults",
                SDL_GetError());
        }
        return "";
    }
    std::string path = std::string(folder) + "a11y.ini";
    SDL_free(folder);
    return path;
}

/* What the switches leave on, to the features that act on it: steering
 * to the screens that steer, Accessibility's part of the cues to the cue
 * system. Speech the switches set themselves. */
void apply_switches(const a11y::SwitchState& state) {
    s_css->set_steering(state.steering);
    s_sss->set_steering(state.steering);
    if (s_cue_system != nullptr) {
        s_cue_system->set_allowed(s_switches->cues_allowed());
    }
}

/* Every change to the accessibility settings, from the file or a menu, to
 * the features that keep a copy. */
void settings_changed(const std::vector<const a11y::NamedValue*>& changed) {
    if (s_switches != nullptr) {
        const bool speech_was_on = s_switches->state().speech;
        const a11y::SwitchState& now = s_switches->update();
        apply_switches(now);
        if (now.speech && !speech_was_on) {
            /* The RmlUi pages see speech come back by themselves and
             * announce their focus (PageReader::follow_focus); the main
             * menu tree says its screen afresh on its next frame. */
            s_menu_speech->forget();
        }
    }
    if (s_cue_system != nullptr) {
        s_cue_system->settings_changed(changed);
    }
    if (s_voice_settings != nullptr) {
        s_voice_settings->changed(changed, a11y::Clock::now());
    }
}

/* The voices' frame and their rows', at every frame boundary of the
 * launcher (`launcher`) and the game, outside the simulation. */
void voices_frame(bool launcher, bool port_menu_open) {
    if (s_voices != nullptr) {
        s_voices->frame(a11y::Clock::now(), port_menu_open);
    }
    if (s_voice_settings != nullptr) {
        s_voice_settings->frame(a11y::Clock::now(), launcher);
    }
}

/* The gate every hook from game code passes through. While rollback re-runs
 * frames (pc_net_resim), each hook is reached again for a frame that was
 * already played, so the fork stays silent rather than repeat itself. Also
 * false outside init and shutdown. */
bool game_hook_may_speak() {
    return s_speech != nullptr && !pc_net_resim();
}

/* The screen on now takes the D-pad for the fork: character select's hand
 * or stage select's cursor steered, or results' reading cursor. */
bool screen_takes_dpad() {
    switch (static_cast<a11y::SceneKind>(s_scene)) {
    case a11y::SceneKind::GS_CSS:
        return s_css != nullptr && s_css->steers();
    case a11y::SceneKind::GS_SSS:
        return s_sss != nullptr && s_sss->steers();
    case a11y::SceneKind::GS_RESULTS:
        return s_results != nullptr && s_results->takes_dpad();
    default:
        return false;
    }
}

/* The stick the screen on now asks for, handed over once; false for none. */
bool take_screen_stick(a11y::Stick* out) {
    switch (static_cast<a11y::SceneKind>(s_scene)) {
    case a11y::SceneKind::GS_CSS:
        return s_css != nullptr && s_css->take_stick(out);
    case a11y::SceneKind::GS_SSS:
        return s_sss != nullptr && s_sss->take_stick(out);
    default:
        return false;
    }
}

/* Taken by the next frame read even while silent, so a refusal in a re-run
 * frame is said once. */
bool take_stage_switch_refused() {
    bool refused = s_stage_switch_refused;
    s_stage_switch_refused = false;
    return refused;
}

/* The menu reader's frame: the main menu tree's, or the rules screens'
 * inside character select, which set no centre text (-1); the main menu's
 * last one would be looked up in character select's string table. */
void feed_menu_speech(int center_text, bool refused) {
    A11yMenuState state;
    a11y_game_menu_state(center_text, &state);
    state.refused = refused;
    s_menu_speech->frame(state);
}

/* What a scene's arrival says, for pc_a11y_scene_entered. */
void announce_scene(int previous_scene, int css_exit, int scene_kind) {
    if (!game_hook_may_speak()) {
        return;
    }
    /* Arriving on the screen and entry just left, as when character select
     * goes back, is still an arrival. */
    s_menu_speech->forget();
    s_css->forget();
    s_sss->forget();
    if (scene_kind == static_cast<int>(a11y::SceneKind::GS_ONLINE_LOBBY)) {
        /* LAN play's lobby and Direct Connect speak their own openings on
         * their first frames. */
        a11y::LobbyArrival arrival = a11y::lobby_arrival(previous_scene, css_exit);
        if (a11y_game_lan_play()) {
            s_lobby->entered(arrival);
            return;
        }
        if (a11y_game_direct_connect()) {
            s_direct->entered(arrival);
            return;
        }
    }
    s_scene_speech->entered(scene_kind);
}

}  // namespace

extern "C" void pc_a11y_init(void) {
    /* The drivers' event pipe, with speech on or off. */
    a11y::event_pipe_start();
    if (s_speech != nullptr) {
        return;
    }
    a11y::Config config = a11y::config_from_environment();
    /* Silent until the switches are read from the accessibility settings:
     * a player who saved Accessibility off hears nothing, and the screen
     * reader is never started. */
    config.speech = false;
    s_speech = std::make_unique<a11y::Speech>(config, a11y::make_screen_reader_bridge());
    s_speech->init();
    s_scene_speech = std::make_unique<a11y::SceneSpeech>(*s_speech);
    a11y::GameTextSource game_text;
    game_text.pal = pc_region_pal;
    game_text.resolve = a11y_game_resolve;
    s_menu_speech = std::make_unique<a11y::MenuSpeech>(*s_speech, game_text);
    s_css = std::make_unique<a11y::CssReader>(*s_speech, false);
    s_sss = std::make_unique<a11y::SssReader>(*s_speech, false);
    s_lobby = std::make_unique<a11y::LobbySpeech>(*s_speech);
    s_direct = std::make_unique<a11y::DirectSpeech>(*s_speech);
    s_training = std::make_unique<a11y::TrainingSpeech>(*s_speech);
    s_results = std::make_unique<a11y::ResultsSpeech>(*s_speech, game_text);
    s_memcard =
        std::make_unique<a11y::MemcardSpeech>(*s_speech, game_text, a11y_game_memcard_string);
    s_match_announcer = std::make_unique<a11y::MatchAnnouncer>(*s_speech);
    s_settings = std::make_unique<a11y::AccessibilitySettings>(
        *s_speech, settings_file_path(config), config.log);
    /* The switches first: the first rows and the file's first groups, the
     * voices' rows after Speech, and the match announcements' after them. */
    s_switches = std::make_unique<a11y::Switches>(s_settings->model(), *s_speech, config.log, [] {
        s_voice_settings = std::make_unique<a11y::VoiceSettings>(s_settings->model(), *s_speech);
        s_match_settings = std::make_unique<a11y::MatchAnnouncementSettings>(s_settings->model());
    });
    s_cue_system = a11y::CueSystem::open(config, *s_speech, *s_settings, s_master_volume);
    /* Attached to speech before the switches turn it on, which starts the
     * voices' worker. */
    s_voices = std::make_unique<a11y::Voices>(*s_speech, s_cue_system->engine(),
        a11y::make_voice_bridge(), config.log, config.voice_preview);
    if (!s_cue_system->sounding()) {
        s_voices->no_audio_output();
    }
    /* Once every feature has registered its values. */
    a11y::Switches::override_from(config, *s_settings);
    s_settings->listen(settings_changed);
    s_settings->load(a11y::Clock::now());
    s_voice_settings->attach(*s_voices);
    apply_switches(s_switches->start());
    s_speech->announce("Non-Visual Melee ready", a11y::Mode::interrupt);
    if (!s_cue_system->sounding()) {
        s_speech->announce("Audio cues could not start.", a11y::Mode::queue);
    }
    if (!s_settings->problem().empty()) {
        /* Found as the file was read, before speech was on. */
        s_speech->announce(s_settings->problem(), a11y::Mode::queue);
    }
    /* The Accessibility category's rows, built as the launcher's and the
     * port menu's pages load. The port menu's sounds are the base port's
     * own, played from the same place its rows play them, outside the
     * simulation, so they are counted in no netplay tick's sound order. */
    a11y::settings_pages_start(
        *s_settings, a11y::SettingsPageGame{
                         [](std::string_view group) {
                             /* A voice's Preview, else a cue's. */
                             if (s_voice_settings != nullptr && s_voice_settings->preview(group)) {
                                 return;
                             }
                             if (s_cue_system != nullptr) {
                                 s_cue_system->preview(group, a11y::Clock::now());
                             }
                         },
                         [] {
                             if (s_cue_system != nullptr) {
                                 s_cue_system->stop_preview(a11y::Clock::now());
                             }
                         },
                         [](a11y::MenuSound sound) { lbAudioAx_80024030(static_cast<int>(sound)); },
                     });
    a11y::launcher_speech_start(*s_speech);
    a11y::port_menu_speech_start(*s_speech);
}

extern "C" void pc_a11y_shutdown(void) {
    if (s_speech == nullptr) {
        return;
    }
    a11y::port_menu_speech_stop();
    a11y::launcher_speech_stop();
    a11y::settings_pages_stop();
    s_voice_settings.reset();
    s_voices.reset();
    s_cue_system.reset();
    s_switches.reset();
    s_match_settings.reset();
    s_settings.reset();
    s_match_announcer.reset();
    s_results.reset();
    s_memcard.reset();
    s_training.reset();
    s_lobby.reset();
    s_direct.reset();
    s_sss.reset();
    s_css.reset();
    s_menu_speech.reset();
    s_scene_speech.reset();
    s_speech->shutdown();
    s_speech.reset();
}

extern "C" void pc_a11y_launcher_frame(void) {
    if (s_settings != nullptr) {
        /* A hand edit shows in an open menu. */
        s_settings->poll(a11y::Clock::now());
    }
    /* The Accessibility category's values written before the reader reads
     * the page, and a preview stopped once its row has lost focus. */
    a11y::settings_pages_frame(a11y::SettingsHost::launcher, true);
    if (s_cue_system != nullptr) {
        s_cue_system->preview_frame(a11y::Clock::now());
    }
    voices_frame(true, false);
    a11y::launcher_speech_frame(a11y::Clock::now());
}

extern "C" bool pc_a11y_launcher_step(int direction) {
    return a11y::launcher_speech_step(direction);
}

extern "C" void pc_a11y_launcher_enter_text(void) {
    a11y::launcher_speech_enter();
}

extern "C" bool pc_a11y_launcher_back(void) {
    return a11y::launcher_speech_back();
}

/* Speech is behind the game hooks' gate, which never silences it wrongly: a
 * re-run frame is simulated inside a tick, never at the frame boundary where
 * the menu's frame runs, and while the menu is open the game stands still.
 * Under MELEE_NET_SYNCTEST, a debug mode, the gate silences every hook, this
 * one too. */
extern "C" void pc_a11y_port_menu_frame(bool open) {
    if (open && s_cue_system != nullptr) {
        /* Before the gate: stopping cues is never wrong, and before the
         * menu's opening is spoken. */
        s_cue_system->silence_for_port_menu();
    }
    /* Before the gate too: the Accessibility category's values, and a
     * preview stopped once its row has lost focus or the menu has closed. */
    a11y::settings_pages_frame(a11y::SettingsHost::port_menu, open);
    if (s_cue_system != nullptr) {
        /* At every frame boundary, the menu open or not, outside the
         * simulation: a preview never touches the game. */
        s_cue_system->preview_frame(a11y::Clock::now());
    }
    /* The voices' lines play in turn here, outside the simulation too; the
     * menu opening drops the lines waiting. */
    voices_frame(false, open);
    if (!game_hook_may_speak()) {
        return;
    }
    if (open) {
        /* A hand edit shows in the open menu; the game stands still, so no
         * tick polls. */
        s_settings->poll(a11y::Clock::now());
    }
    a11y::port_menu_speech_frame(open, pc_exit_requested, a11y::Clock::now());
}

extern "C" bool pc_a11y_port_menu_step(int direction) {
    return game_hook_may_speak() && a11y::port_menu_speech_step(direction);
}

extern "C" void pc_a11y_scene_entered(int mode_kind, int scene_kind) {
    (void)mode_kind; /* for mode-specific scene names, later */
    int previous_scene = s_scene;
    int css_exit = s_css_exit;
    s_scene = scene_kind;
    s_css_exit = A11Y_CSS_STAYING;
    s_css_to_rules = false;
    s_css_menu_fed = false;
    if (s_training != nullptr && scene_kind == static_cast<int>(a11y::SceneKind::GS_TRAINING)) {
        /* The scene's start has just reset the Training menu: a new
         * session, whose first opening says the keys. */
        s_training->session_started();
    }
    if (s_match_announcer != nullptr) {
        /* Each match is a scene of its own, sudden death and each of a
         * single-player mode's stages included. */
        s_match_announcer->scene_entered();
    }
    if (s_results != nullptr && scene_kind == static_cast<int>(a11y::SceneKind::GS_RESULTS)) {
        /* Silent on entry: the reader speaks once the announcer has named
         * the winner. */
        s_results->visit_started();
    }
    if (s_memcard != nullptr && scene_kind == static_cast<int>(a11y::SceneKind::GS_MEMCARD)) {
        /* Silent on entry: the scene's first prompt is its opening. */
        s_memcard->visit_started();
    }
    announce_scene(previous_scene, css_exit, scene_kind);
    if (s_voice_settings != nullptr && game_hook_may_speak()) {
        /* Queued behind the first scene's announcement, which interrupts:
         * what the screen reader needs to know of the voices. */
        s_voice_settings->scene_announced();
    }
    if (s_cue_system != nullptr) {
        /* After the scene's announcement, which interrupts. */
        s_cue_system->scene_entered();
    }
    if (s_settings != nullptr) {
        /* After the announcement too, so an error in the file is queued
         * behind it; a screen that speaks its opening on its first frame
         * still cuts it off. */
        s_settings->poll(a11y::Clock::now());
    }
}

extern "C" void pc_a11y_memcard_prompt(int kind, int text_id) {
    if (!game_hook_may_speak()) {
        return;
    }
    /* Said by a later frame, once it has stayed up a whole frame. */
    s_memcard->prompt(kind, text_id);
}

extern "C" void pc_a11y_memcard_frame(int hovered_choice) {
    a11y::menu_text_dump_once(A11Y_MEMCARD_FONT);
    if (!game_hook_may_speak()) {
        return;
    }
    s_memcard->frame(hovered_choice);
}

extern "C" void pc_a11y_master_volume(float volume) {
    s_master_volume = volume;
    if (s_cue_system != nullptr) {
        s_cue_system->set_master_volume(volume);
    }
}

extern "C" void pc_a11y_menu_frame(void) {
    a11y::menu_text_dump_once();
    bool refused = take_stage_switch_refused();
    if (!game_hook_may_speak()) {
        return;
    }
    feed_menu_speech(s_center_text, refused);
}

extern "C" void pc_a11y_stage_switch_refused(void) {
    s_stage_switch_refused = true;
}

extern "C" void pc_a11y_menu_center_text(int string_number) {
    /* Read by the next menu frame, which also sees the screen the same
     * press opened; nothing is spoken here. */
    s_center_text = string_number;
}

extern "C" void pc_a11y_css_frame(const CSSData* css, const CSSDoorsData* doors,
    const CSSIcon* icons, const CSSTag* tags, const CSSDoorsMisc* misc, const CSSDoorsData2* data2,
    HSD_JObj* model_root, int hand_count, int pending_exit, int ready) {
    a11y::menu_text_dump_once();
    s_css_exit = pending_exit;
    if (pending_exit == A11Y_CSS_TO_RULES) {
        s_css_to_rules = true;
    } else if (pending_exit != A11Y_CSS_AWAY) {
        s_css_to_rules = false;
    }
    bool refused = take_stage_switch_refused();
    if (!game_hook_may_speak()) {
        return;
    }
    /* Online, the local player's controller is always port 1, but they may
     * be any player in the game. */
    int local_port = pc_net_active() ? pc_net_local_player() : 0;
    A11yCssScreen screen{
        css, doors, icons, tags, misc, data2, model_root, hand_count, pending_exit, ready};
    s_css->frame(screen, local_port, pc_net_active());
    /* The rules screens open inside this scene, run by the main menu tree's
     * code, and this hook keeps running meanwhile: the menu reader speaks
     * for them. The frame that asks for them still shows the menu tree's
     * last screen, and once they close it stays behind in mn_804A04F0, so
     * the pending exit is the gate (ADR-0003). */
    bool rules_open = pending_exit == A11Y_CSS_AWAY && s_css_to_rules;
    if (rules_open && !s_css_menu_fed) {
        /* Each visit is an arrival. */
        s_menu_speech->forget();
    }
    s_css_menu_fed = rules_open;
    if (rules_open) {
        feed_menu_speech(-1, refused);
    }
}

extern "C" void pc_a11y_css_hand(int hand, int state, int held, float x, float y) {
    if (!game_hook_may_speak() || hand < 0 || hand >= A11Y_CSS_SLOTS) {
        return;
    }
    /* Read by the next frame hook, which sees the player slots this update
     * left behind. The local hand also steers here, once per simulated
     * frame. */
    s_css->hand(hand, A11yCssHandReport{true, state, held, x, y});
}

extern "C" void pc_a11y_css_coin(int slot, float x, float y) {
    if (!game_hook_may_speak() || slot < 0 || slot >= A11Y_CSS_SLOTS) {
        return;
    }
    /* Read by the next frame hook: a step finds a coin to pick up by it. */
    s_css->coin(slot, x, y);
}

extern "C" void pc_a11y_sss_frame(const SSSData* data, const void* tiles, int tile_size,
    int hovered, int pick, int frozen, int buttons, int owner, int local_pick, int remote_pick) {
    if (!game_hook_may_speak()) {
        return;
    }
    A11ySssScreen screen{
        data, tiles, tile_size, hovered, pick, frozen, buttons, owner, local_pick, remote_pick};
    s_sss->frame(screen, pc_net_active());
}

extern "C" void pc_a11y_sss_resolved(int stage_tile, int local_pick, int remote_pick) {
    if (!game_hook_may_speak()) {
        return;
    }
    s_sss->resolved(stage_tile, local_pick, remote_pick);
}

extern "C" void pc_a11y_sss_cursor(float x, float y, int stick_x, int stick_y) {
    if (!game_hook_may_speak()) {
        return;
    }
    /* Once per simulated frame: steps, and the glide's next frame. */
    s_sss->cursor(a11y::Point{x, y}, stick_x, stick_y);
}

extern "C" void pc_a11y_lobby_frame(const OnlineLobbyView* view) {
    if (!game_hook_may_speak()) {
        return;
    }
    A11yLobbyState state;
    a11y_game_lobby_state(view, &state);
    s_lobby->frame(state);
    s_direct->frame(state);
}

extern "C" void pc_a11y_match_tick(uint64_t held_mask) {
    bool results = s_scene == static_cast<int>(a11y::SceneKind::GS_RESULTS);
    if (results) {
        a11y::menu_text_dump_once();
    }
    if (!game_hook_may_speak()) {
        return;
    }
    if (results) {
        /* After the tick's procs, the scene's own update included. Online,
         * the local player's controller is port 1, but they may be any
         * player in the game. */
        A11yResults snapshot;
        a11y_game_results(pc_net_active() ? pc_net_local_player() : 0, &snapshot);
        snapshot.online = pc_net_active();
        s_results->tick(snapshot);
    }
    if (s_scene == static_cast<int>(a11y::SceneKind::GS_TRAINING)) {
        /* After the tick's procs, the Training menu's own update included,
         * so the snapshot is the tick's finished state. It runs on paused
         * ticks too: the menu is only open while the match is paused. */
        A11yTrainingMenu menu;
        a11y_game_training_menu(&menu);
        s_training->tick(menu, a11y::Clock::now());
    }
    int local_port = pc_net_active() ? pc_net_local_player() : 0;
    if (s_match_announcer != nullptr && s_match_settings != nullptr) {
        /* In every scene: outside a match the snapshot says so. */
        A11yMatchHud hud;
        a11y_game_match_hud(local_port, pc_net_active(), &hud);
        s_match_announcer->tick(hud, s_match_settings->values(), a11y::Clock::now());
    }
    if (s_cue_system != nullptr) {
        s_cue_system->tick(local_port, pc_net_active(), held_mask);
    }
}

extern "C" bool pc_a11y_pad(PADStatus* pad) {
    a11y::Stick stick;
    if (!take_screen_stick(&stick)) {
        return false;
    }
    /* As the pad's other sources merge: each axis the one pushed further,
     * so a player's own stick wins and the glide gives way. */
    if (std::abs(stick.x) > std::abs(pad->stickX)) {
        pad->stickX = static_cast<s8>(stick.x);
    }
    if (std::abs(stick.y) > std::abs(pad->stickY)) {
        pad->stickY = static_cast<s8>(stick.y);
    }
    return true;
}

extern "C" uint16_t pc_a11y_chat_buttons(uint16_t buttons) {
    /* Not gated on rollback: chat runs once per video frame, outside the
     * simulation, and what it is handed changes nothing the peer sees but
     * phrases. */
    if (!screen_takes_dpad()) {
        return buttons;
    }
    constexpr uint16_t dpad = PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_UP;
    return static_cast<uint16_t>(buttons & ~dpad);
}
