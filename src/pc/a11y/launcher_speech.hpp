/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Launcher speech: reads the launcher page (resources/launcher.rml) aloud by
 * comparing it with the previous frame. An RmlUi plugin starts and stops the
 * reader as the page loads and unloads; the launcher's loop drives it through
 * pc_a11y_launcher_frame. docs/a11y/rmlui-screens.md explains the mechanism,
 * docs/adr/0002-launcher-speech-reads-the-page.md why. */
#pragma once
#include <chrono>

namespace a11y {

class Speech;

using Clock = std::chrono::steady_clock;

/* The two numbers to tune by ear. A watched text is spoken once it has stayed
 * the same this long, so passing messages are skipped. */
constexpr std::chrono::milliseconds kSettleTime{250};
/* A text whose only change is its percentage is progress, spoken each time it
 * crosses the next multiple of this, and at 100%. */
constexpr int kProgressStep = 20;

/* Registers the plugin; speech must outlive the matching stop. Call before or
 * after RmlUi starts. */
void launcher_speech_start(Speech& speech);
/* Unregisters the plugin and drops the reader. Call before speech shuts down;
 * RmlUi may shut down before or after. */
void launcher_speech_stop();
/* One turn of the launcher's loop, after RmlUi has updated the page. */
void launcher_speech_frame(Clock::time_point now);

}  // namespace a11y
