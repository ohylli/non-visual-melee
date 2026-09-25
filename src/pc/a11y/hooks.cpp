/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "a11y_hooks.h"
#include "launcher_speech.hpp"
#include "speech.hpp"
#include <memory>

namespace {

/* Constant-initialised empty, so nothing runs before main; created by
 * pc_a11y_init and destroyed by pc_a11y_shutdown. */
std::unique_ptr<a11y::Speech> s_speech;

}  // namespace

extern "C" void pc_a11y_init(void) {
    if (s_speech != nullptr) {
        return;
    }
    s_speech = std::make_unique<a11y::Speech>(
        a11y::config_from_environment(), a11y::make_screen_reader_bridge());
    s_speech->init();
    s_speech->announce("Non-Visual Melee ready", a11y::Mode::interrupt);
    a11y::launcher_speech_start(*s_speech);
}

extern "C" void pc_a11y_shutdown(void) {
    if (s_speech == nullptr) {
        return;
    }
    a11y::launcher_speech_stop();
    s_speech->shutdown();
    s_speech.reset();
}

extern "C" void pc_a11y_launcher_frame(void) {
    a11y::launcher_speech_frame(a11y::Clock::now());
}
