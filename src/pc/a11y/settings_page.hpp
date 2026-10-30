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
 * and play and stop a cue's preview.
 *
 * The rows are in submenus
 * (docs/adr/0009-the-accessibility-settings-split-into-submenus-inside-the-one-category.md):
 * every level is built at once, only the open one shown, with a title line
 * naming it at the top of the page (#a11y-menu-title, aria-hidden, so the
 * readers leave it out of the lines they read). A row opening a submenu is a
 * button of the class "submenu". Enter, A, a click or Right on it opens the
 * submenu on its first row; Back (pc_a11y_settings_back), or Left on a row
 * holding no value, goes back to it; and focus anywhere outside the open
 * level's rows, or the page hidden, shows the top level again. The launcher
 * walks the open level's rows by itself, pc_a11y_settings_rows listing them;
 * the port menu copies its column once, so inside a submenu
 * settings_pages_step moves for it. Game thread only. */
#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>

namespace Rml {
class ElementDocument;
}

namespace a11y {

using Clock = std::chrono::steady_clock;

class AccessibilitySettings;
struct NamedMenu;
struct ShownRow;

/* The port menu's own sounds, numbered as Melee's menu bank plays them
 * (lbAudioAx_80024030): copies of PortMenu's SFX_BACK, SFX_CONFIRM and
 * SFX_MOVE in src/pc/launcher.cpp, to check at a base merge. */
enum class MenuSound {
    back = 0,
    confirm = 1,
    move = 2,
};

/* What the rows ask of the game. */
struct SettingsPageGame {
    /* A Preview row's press, by its action's group: a voice's sample
     * (VoiceSettings::preview) or a cue's preview (CueSystem::preview, the
     * group being the cue's id). The stop is a cue preview's
     * (CueSystem::stop_preview), nothing when none plays: a voice's sample
     * is a line of a second, which plays out, and a new one cuts it. */
    std::function<void(std::string_view group)> preview;
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
 * "a11y-cues-volume", "a11y-gap_warning-preview", "a11y-cues-settings". A
 * slider's shown value is beside it, as the base port's are, at the id and
 * "-val". */
std::string settings_row_id(const ShownRow& row);

/* Registers the plugin; the settings and the game's functions must outlive
 * the matching stop. Call once every feature has registered its values, and
 * before RmlUi loads either page. */
void settings_pages_start(AccessibilitySettings& settings, SettingsPageGame game);
/* Unregisters the plugin and drops the pages, stopping a preview one of
 * them started. RmlUi may shut down before or after. */
void settings_pages_stop();
/* Once a frame of the host's loop, after it has reacted to the frame's
 * input: the rows show the values the game uses (a hand edit, an override
 * lifted), and a cue's preview a row started stops once that row has lost focus,
 * the page has gone, or the port menu is closed (`open` false). */
void settings_pages_frame(SettingsHost host, bool open);
/* Up (-1) or Down (1) in the host's page, before the page moves focus
 * itself: inside a submenu, focus goes round its rows and this returns
 * true; on the top level, false, and the page moves as it does. For the
 * port menu, whose column of rows is a copy of the top level's. */
bool settings_pages_step(SettingsHost host, int direction);

/* For a page's reader: the name of a submenu just entered, which the reader
 * says before the announcement of its first row, where the page has put
 * focus within the same press. Asked of the page rather than read off the
 * title line, whose text is for the eye. */
class SubmenuEntry {
public:
    /* Once a frame of the reader's: the label of the submenu the document's
     * Accessibility category has gone into since the last call, from the
     * top level or from the submenu it is inside: "Cue settings", "Voice
     * settings". Empty when it has not, as on going back up, where the
     * opening row's own announcement says where focus is. */
    std::string frame(const Rml::ElementDocument& document);

private:
    const NamedMenu* m_open = nullptr; /* the submenu open at the last call */
};

}  // namespace a11y
