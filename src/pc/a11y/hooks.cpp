/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "a11y_hooks.h"
#include "css_reader.hpp"
#include "cue_engine.hpp"
#include "cue_output.hpp"
#include "game_access.h"
#include "game_text.hpp"
#include "launcher_speech.hpp"
#include "lobby_speech.hpp"
#include "menu_speech.hpp"
#include "menu_text.hpp"
#include "pc/net.h"
#include "pc/pc.h"
#include "pc/region.h"
#include "scene_speech.hpp"
#include "speech.hpp"
#include "sss_reader.hpp"
#include <cstdlib>
#include <dolphin/pad.h>
#include <memory>
#include <string>

namespace {

/* Constant-initialised empty, so nothing runs before main; created by
 * pc_a11y_init and destroyed by pc_a11y_shutdown. */
std::unique_ptr<a11y::Speech> s_speech;
std::unique_ptr<a11y::SceneSpeech> s_scene_speech;
std::unique_ptr<a11y::MenuSpeech> s_menu_speech;
std::unique_ptr<a11y::CssReader> s_css;
std::unique_ptr<a11y::SssReader> s_sss;
std::unique_ptr<a11y::LobbySpeech> s_lobby;
/* The cue engine and its output, both null while cues are off or the output
 * could not open. The output goes first: its callback renders the engine. */
std::unique_ptr<a11y::CueEngine> s_cues;
std::unique_ptr<a11y::CueOutput> s_cue_output;
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

/* The gate every hook from game code passes through. While rollback re-runs
 * frames (pc_net_resim), each hook is reached again for a frame that was
 * already played, so the fork stays silent rather than repeat itself. Also
 * false outside init and shutdown. */
bool game_hook_may_speak() {
    return s_speech != nullptr && !pc_net_resim();
}

/* The screen on now steers a cursor with the D-pad: character select's hand
 * or stage select's cursor. */
bool screen_steers() {
    switch (static_cast<a11y::SceneKind>(s_scene)) {
    case a11y::SceneKind::GS_CSS:
        return s_css != nullptr && s_css->steers();
    case a11y::SceneKind::GS_SSS:
        return s_sss != nullptr && s_sss->steers();
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

/* Opens the cue engine as the game starts, not on first use: opening the
 * stream takes part of a frame (ADR-0005). */
void open_cues(const a11y::Config& config) {
    if (!config.cues) {
        return;
    }
    a11y::CueEngineOptions options;
    options.log = config.log;
    options.dump = config.cue_dump;
    options.trace = config.cue_trace;
    s_cues = std::make_unique<a11y::CueEngine>(options);
    s_cues->set_master_volume(s_master_volume);
    std::string why;
    s_cue_output = a11y::CueOutput::open(*s_cues, config.log, why);
    if (s_cue_output == nullptr) {
        s_cues.reset();
        if (config.log) {
            pc_log_line("[a11y] cue engine could not start: %s", why.c_str());
        }
        s_speech->announce("Audio cues could not start.", a11y::Mode::queue);
    }
}

}  // namespace

extern "C" void pc_a11y_init(void) {
    if (s_speech != nullptr) {
        return;
    }
    a11y::Config config = a11y::config_from_environment();
    s_speech = std::make_unique<a11y::Speech>(config, a11y::make_screen_reader_bridge());
    s_speech->init();
    s_speech->announce("Non-Visual Melee ready", a11y::Mode::interrupt);
    s_scene_speech = std::make_unique<a11y::SceneSpeech>(*s_speech);
    a11y::GameTextSource game_text;
    game_text.pal = pc_region_pal;
    game_text.resolve = a11y_game_resolve;
    s_menu_speech = std::make_unique<a11y::MenuSpeech>(*s_speech, game_text);
    s_css = std::make_unique<a11y::CssReader>(*s_speech, config.steer);
    s_sss = std::make_unique<a11y::SssReader>(*s_speech, config.steer);
    s_lobby = std::make_unique<a11y::LobbySpeech>(*s_speech);
    open_cues(config);
    a11y::launcher_speech_start(*s_speech);
}

extern "C" void pc_a11y_shutdown(void) {
    if (s_speech == nullptr) {
        return;
    }
    a11y::launcher_speech_stop();
    s_cue_output.reset();
    s_cues.reset();
    s_lobby.reset();
    s_sss.reset();
    s_css.reset();
    s_menu_speech.reset();
    s_scene_speech.reset();
    s_speech->shutdown();
    s_speech.reset();
}

extern "C" void pc_a11y_launcher_frame(void) {
    a11y::launcher_speech_frame(a11y::Clock::now());
}

extern "C" void pc_a11y_scene_entered(int mode_kind, int scene_kind) {
    (void)mode_kind; /* for mode-specific scene names, later */
    int previous_scene = s_scene;
    int css_exit = s_css_exit;
    s_scene = scene_kind;
    s_css_exit = A11Y_CSS_STAYING;
    s_css_to_rules = false;
    s_css_menu_fed = false;
    if (s_cues != nullptr) {
        /* Every cue belongs to the screen it plays on. */
        s_cues->stop_all();
    }
    if (!game_hook_may_speak()) {
        return;
    }
    /* Arriving on the screen and entry just left, as when character select
     * goes back, is still an arrival. */
    s_menu_speech->forget();
    s_css->forget();
    s_sss->forget();
    if (scene_kind == static_cast<int>(a11y::SceneKind::GS_ONLINE_LOBBY) && a11y_game_lan_play()) {
        /* LAN play's lobby speaks its own opening on its first frame. */
        s_lobby->entered(a11y::lobby_arrival(previous_scene, css_exit));
        return;
    }
    s_scene_speech->entered(scene_kind, pc_net_active());
}

extern "C" void pc_a11y_master_volume(float volume) {
    s_master_volume = volume;
    if (s_cues != nullptr) {
        s_cues->set_master_volume(volume);
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
    if (!screen_steers()) {
        return buttons;
    }
    constexpr uint16_t dpad = PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_UP;
    return static_cast<uint16_t>(buttons & ~dpad);
}
