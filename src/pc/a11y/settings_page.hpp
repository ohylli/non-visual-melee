/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The Accessibility category of the launcher's Settings and of the port menu
 * (docs/adr/0007-the-accessibility-category-is-wired-into-the-base-ports-menus.md; primer
 * docs/a11y/rmlui-screens.md). The base port's files hold only its wiring:
 * the category's id and button, an empty page (#page-accessibility), and
 * hand-offs to the pc_a11y_settings_* hooks (a11y_hooks.h, defined in
 * settings_page.cpp) at the top of the handlers that act on an element id.
 *
 * An RmlUi plugin builds the rows into the empty page as either document
 * loads, from the accessibility settings' shown rows
 * (AccessibilitySettings::shown), in the base port's own markup classes, so
 * the pages' styles and the readers apply unchanged; a new cue or a newly
 * shown tuning value changes nothing here. The rows write their own values,
 * save through the settings, play the port menu's own sounds as its rows do,
 * and play and stop a cue's preview. Game thread only. */
#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>

namespace a11y {

using Clock = std::chrono::steady_clock;

class AccessibilitySettings;
struct ShownRow;

/* The port menu's own sounds, numbered as Melee's menu bank plays them
 * (lbAudioAx_80024030, as PortMenu in src/pc/launcher.cpp plays them). */
enum class MenuSound {
    back = 0,
    confirm = 1,
    move = 2,
};

/* What the rows ask of the game. */
struct SettingsPageHost {
    /* A cue's preview, by the cue's id (CueSystem::preview), and its stop
     * (CueSystem::stop_preview), nothing when none plays. */
    std::function<void(std::string_view cue)> preview;
    std::function<void()> stop_preview;
    /* A sound of the port menu's; the launcher plays none, as the game has
     * loaded none yet. */
    std::function<void(MenuSound)> port_menu_sound;
};

/* The page a frame is for. */
enum class SettingsHost {
    launcher,
    port_menu,
};

/* A row's control id: "a11y-", then the row's key with its dots as dashes:
 * "a11y-cues-volume", "a11y-gap_warning-preview". A slider's shown value is
 * beside it, as the base port's are, at the id and "-val". */
std::string settings_row_id(const ShownRow& row);

/* Registers the plugin; the settings and the host's functions must outlive
 * the matching stop. Call once every feature has registered its values, and
 * before RmlUi loads either page. */
void settings_pages_start(AccessibilitySettings& settings, SettingsPageHost host);
/* Unregisters the plugin and drops the pages, stopping a preview one of
 * them started. RmlUi may shut down before or after. */
void settings_pages_stop();
/* Once a frame of the host's loop, after it has reacted to the frame's
 * input: the rows show the values the game uses (a hand edit, an override
 * lifted), and a preview a row started stops once that row has lost focus,
 * the page has gone, or the port menu is closed (`open` false). */
void settings_pages_frame(SettingsHost host, bool open);

}  // namespace a11y
