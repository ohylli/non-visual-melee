/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Port menu speech: reads the port menu (resources/port-menu.rml, F1 or Back
 * on a controller) aloud the way the launcher is read, on the same shared
 * reader. An RmlUi plugin starts the reader as the page loads, when the game
 * starts; the menu's frame drives it through pc_a11y_port_menu_frame, with
 * the menu open or not. docs/a11y/rmlui-screens.md explains it. */
#pragma once
#include "rmlui_reader.hpp"

namespace a11y {

class Speech;

/* Registers the plugin; speech must outlive the matching stop. Call before or
 * after RmlUi starts. */
void port_menu_speech_start(Speech& speech);
/* Unregisters the plugin and drops the reader. Call before speech shuts down;
 * RmlUi may shut down before or after. */
void port_menu_speech_stop();
/* Once a frame of the game loop, after the menu has reacted to the frame's
 * input, whether the menu is open or not: says the opening as it opens,
 * "Back to game" as it closes, and in between follows the page. */
void port_menu_speech_frame(bool open, Clock::time_point now);
/* Up (-1) or Down (1), before the menu moves focus. Returns true when port
 * menu speech used the press for the lines of a tab's page, so focus must not
 * move. */
bool port_menu_speech_step(int direction);

}  // namespace a11y
