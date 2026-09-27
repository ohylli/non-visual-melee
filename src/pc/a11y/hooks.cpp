/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "a11y_hooks.h"
#include "game_access.h"
#include "game_text.hpp"
#include "launcher_speech.hpp"
#include "menu_speech.hpp"
#include "menu_text.hpp"
#include "pc/net.h"
#include "pc/region.h"
#include "scene_speech.hpp"
#include "speech.hpp"
#include <memory>

namespace {

/* Constant-initialised empty, so nothing runs before main; created by
 * pc_a11y_init and destroyed by pc_a11y_shutdown. */
std::unique_ptr<a11y::Speech> s_speech;
std::unique_ptr<a11y::SceneSpeech> s_scene_speech;
std::unique_ptr<a11y::MenuSpeech> s_menu_speech;

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
    s_speech = std::make_unique<a11y::Speech>(
        a11y::config_from_environment(), a11y::make_screen_reader_bridge());
    s_speech->init();
    s_speech->announce("Non-Visual Melee ready", a11y::Mode::interrupt);
    s_scene_speech = std::make_unique<a11y::SceneSpeech>(*s_speech);
    a11y::GameTextSource game_text;
    game_text.pal = pc_region_pal;
    game_text.resolve = a11y_game_resolve;
    s_menu_speech = std::make_unique<a11y::MenuSpeech>(*s_speech, game_text);
    a11y::launcher_speech_start(*s_speech);
}

extern "C" void pc_a11y_shutdown(void) {
    if (s_speech == nullptr) {
        return;
    }
    a11y::launcher_speech_stop();
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
    s_scene_speech->entered(scene_kind);
}

extern "C" void pc_a11y_menu_frame(void) {
    a11y::menu_text_dump_once();
    if (!game_hook_may_speak()) {
        return;
    }
    A11yMenuState state;
    a11y_game_menu_state(&state);
    s_menu_speech->frame(state);
}
