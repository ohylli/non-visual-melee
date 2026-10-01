/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "a11y_hooks.h"
#include "css_reader.hpp"
#include "game_access.h"
#include "game_text.hpp"
#include "launcher_speech.hpp"
#include "menu_speech.hpp"
#include "menu_text.hpp"
#include "pc/net.h"
#include "pc/region.h"
#include "scene_speech.hpp"
#include "speech.hpp"
#include <cstdlib>
#include <dolphin/pad.h>
#include <memory>

namespace {

/* Constant-initialised empty, so nothing runs before main; created by
 * pc_a11y_init and destroyed by pc_a11y_shutdown. */
std::unique_ptr<a11y::Speech> s_speech;
std::unique_ptr<a11y::SceneSpeech> s_scene_speech;
std::unique_ptr<a11y::MenuSpeech> s_menu_speech;
std::unique_ptr<a11y::CssReader> s_css;
/* The centre text a leaf screen last set, kept across scenes: a screen the
 * menu scene opens on, as Multi-Man Melee after its match, sets it before
 * the scene hook runs. */
int s_center_text = -1;

/* The gate every hook from game code passes through. While rollback re-runs
 * frames (pc_net_resim), each hook is reached again for a frame that was
 * already played, so the fork stays silent rather than repeat itself. Also
 * false outside init and shutdown. */
bool game_hook_may_speak() {
    return s_speech != nullptr && !pc_net_resim();
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
    a11y::launcher_speech_start(*s_speech);
}

extern "C" void pc_a11y_shutdown(void) {
    if (s_speech == nullptr) {
        return;
    }
    a11y::launcher_speech_stop();
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
    if (!game_hook_may_speak()) {
        return;
    }
    /* Arriving on the screen and entry just left, as when character select
     * goes back, is still an arrival. */
    s_menu_speech->forget();
    s_css->forget();
    s_scene_speech->entered(scene_kind);
}

extern "C" void pc_a11y_menu_frame(void) {
    a11y::menu_text_dump_once();
    if (!game_hook_may_speak()) {
        return;
    }
    A11yMenuState state;
    a11y_game_menu_state(s_center_text, &state);
    s_menu_speech->frame(state);
}

extern "C" void pc_a11y_menu_center_text(int string_number) {
    /* Read by the next menu frame, which also sees the screen the same
     * press opened; nothing is spoken here. */
    s_center_text = string_number;
}

extern "C" void pc_a11y_css_frame(const CSSData* css, const CSSDoorsData* doors,
    const CSSIcon* icons, const CSSTag* tags, const CSSDoorsMisc* misc, const CSSDoorsData2* data2,
    HSD_JObj* model_root, int hand_count, int pending_exit, int ready) {
    if (!game_hook_may_speak()) {
        return;
    }
    /* Online, the local player's controller is always port 1, but they may
     * be any player in the game. */
    int local_port = pc_net_active() ? pc_net_local_player() : 0;
    A11yCssScreen screen{
        css, doors, icons, tags, misc, data2, model_root, hand_count, pending_exit, ready};
    s_css->frame(screen, local_port, pc_net_active());
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

extern "C" bool pc_a11y_pad(PADStatus* pad) {
    a11y::Stick stick;
    if (s_css == nullptr || !s_css->take_stick(&stick)) {
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
