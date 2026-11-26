/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Every hook from the base port into the accessibility fork. A hook is one
 * call placed where something meaningful to the player happens; the fork
 * decides what to say. `grep -rn pc_a11y_ src --exclude-dir=a11y` lists every
 * call site. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Startup, before the launcher window opens (src/pc/main.c). Starts speech and
 * speaks the proof-of-life announcement. */
void pc_a11y_init(void);
/* First step of shutdown (src/pc/main.c), also reached when the window is
 * closed. */
void pc_a11y_shutdown(void);

/* The game's master volume has been set (pc_audio_set_volume in
 * src/pc/audio.c), 0 to 1 with mute already applied as 0: every launcher and
 * port menu change goes through it. The cue engine's own stream does not see
 * the game's, so it follows the value itself; it is kept even before the
 * engine opens. */
void pc_a11y_master_volume(float volume);

/* Once per turn of the launcher's loop (src/pc/launcher.cpp), after the page
 * has been drawn. Launcher speech compares the page with the last turn here. */
void pc_a11y_launcher_frame(void);

/* The launcher is about to move focus up (-1) or down (1) (focus_step in
 * src/pc/launcher.cpp), whether by arrow key, Tab, D-pad or stick. Returns
 * true when launcher speech used the press for the lines of text on a
 * Settings page, which focus never reaches: to read one, or to come back from
 * them to the focused control. Focus must then stay put. */
bool pc_a11y_launcher_step(int direction);

/* Right, Enter or A on a Settings category found no row of its page to focus
 * (the end of enter_rows in src/pc/launcher.cpp). Launcher speech enters the
 * page if it is text alone, as Controls is, and reads its lines with Up and
 * Down from there; focus stays on the category. */
void pc_a11y_launcher_enter_text(void);

/* Escape, B or Left, before the launcher acts on it (back and horizontal in
 * src/pc/launcher.cpp). Returns true when launcher speech used the press to
 * leave a page of text alone for its category; the launcher must then do
 * nothing. */
bool pc_a11y_launcher_back(void);

/* Once a frame of the game loop (pc_menu_update in src/pc/launcher.cpp, run
 * from pc_frame_boundary), after the port menu has reacted to the frame's
 * input, whether the menu is open or not; open is the menu's own flag,
 * pc_menu_is_open(). While it is open the game stands still, so none of the
 * game's own hooks (scenes, menus, the match tick) run. Port menu speech
 * compares the page with the last frame here, and says the menu's opening and
 * closing. */
void pc_a11y_port_menu_frame(bool open);

/* The port menu is about to move focus up (-1) or down (1) (PortMenu::
 * focus_step in src/pc/launcher.cpp), whether by arrow key, Tab, D-pad or
 * stick. Returns true when port menu speech used the press for the lines of
 * text between a page's rows, as pc_a11y_launcher_step does; focus must then stay
 * put. */
bool pc_a11y_port_menu_step(int direction);

/* A scene has loaded (gm_801A4014 in src/melee/gm/gm_1A3F.c, after the
 * scene's enter function returns) and its first frame is next. mode_kind is
 * the GameModeKind of the mode being run, which during a memory card
 * interruption differs from gm_GetCurrentGameMode(); scene_kind is the
 * GameSceneKind. */
void pc_a11y_scene_entered(int mode_kind, int scene_kind);

/* The memory card scene shows a prompt (gm_801AEE6C in
 * src/melee/gm/gmscmemcard.c): kind 0 a message, 1 Yes and No, 2 OK and
 * Try Again; text_id is its string in the message window's table (SdMsgBox).
 * Nothing is said here: the frame hook says it once it has stayed up a
 * whole frame, so a prompt replaced on the next frame stays silent. */
void pc_a11y_memcard_prompt(int kind, int text_id);

/* Once a frame of the memory card scene (gm_Scene_MemCard_OnFrame), before
 * the frame reads input: hovered_choice is the choice the prompt shows
 * hovered, 0 the left one, 1 the right one. */
void pc_a11y_memcard_frame(int hovered_choice);

/* The unlock notice scene has set up a notice (the end of un_802FE918 in
 * src/melee/if/ifprize.c), its first or the next after A or Start, its
 * text composed: unlock is the notice's unlock number, text_id the string
 * of the prize table (SdPrize) bound to the frame's message. Fork code
 * picks the string to read (a trophy's is composed elsewhere). */
void pc_a11y_unlock_notice(int unlock, int text_id);

/* Once a frame of the main menu tree's scene (mnMain_Scene_OnFrame in
 * src/melee/mn/mnmain.c). Menu speech compares the tree's state with the last
 * frame here (docs/adr/0003-main-menu-tree-is-polled.md). */
void pc_a11y_menu_frame(void);

/* A leaf screen of the main menu tree set its centre text (Menu_InitCenterText
 * in src/melee/mn/inlines.h, used by Sound, Screen display, Language and
 * Multi-Man Melee). string_number is the SdMenu string's NTSC-U number, as the
 * game's code passes it; it names the Sound row or the Multi-Man Melee choice,
 * which those screens keep in private state. The next menu frame reads it. */
void pc_a11y_menu_center_text(int string_number);

/* Once a frame while Language is up (mnLanguage_8024BFE0 in
 * src/melee/mn/mnlanguage.c, before its cooldown check, so even while it
 * ignores input). cursor is the screen's cursor, 0 Japanese and 1 English,
 * kept in state static to that file. The next menu frame reads it. */
void pc_a11y_language_frame(int cursor);

/* Random Stage refused to turn off the last stage that was on and played the
 * refused sound (fn_80235F80 in src/melee/mn/mnstagesw.c); nothing in the
 * screen's state records it. The next menu frame reads it. */
void pc_a11y_stage_switch_refused(void);

/* Character select's state lives in static variables of
 * src/melee/mn/mncharsel.c, so its hooks hand the fork what it reads. Only
 * game_access.c looks inside these types. */
struct CSSData;
struct CSSDoorsData;
struct CSSDoorsData2;
struct CSSDoorsMisc;
struct CSSIcon;
struct CSSTag;
struct HSD_JObj;

/* Once a frame of character select (mnCharSel_Scene_OnFrame), before the
 * hands update: the screen's data, its four player slots, its table of 25
 * portraits, the players' name tag windows, the single-player modes' two
 * tables (the difficulty arrows and the one name box in misc, the stock
 * arrows in data2), the root of the screen's model (where the sliders and
 * name boxes are), the number of hands (4 in VS modes, 1 in single-player
 * modes), the pending exit (nonzero once the screen has begun to leave or
 * asked for the rules screen or name entry) and the Ready to Fight banner
 * (nonzero while shown). Character select speech compares the screen with
 * the last frame here. */
void pc_a11y_css_frame(const struct CSSData* css, const struct CSSDoorsData* doors,
    const struct CSSIcon* icons, const struct CSSTag* tags, const struct CSSDoorsMisc* misc,
    const struct CSSDoorsData2* data2, struct HSD_JObj* model_root, int hand_count,
    int pending_exit, int ready);

/* One hand of character select has updated (the end of
 * mnCharSel_CursorThink). The raw numbers of the hand's struct, which only
 * mncharsel.c defines: its index (the port in VS modes), state, what it
 * holds, and position. The next frame reads them. */
void pc_a11y_css_hand(int hand, int state, int held, float x, float y);

/* One coin of character select has updated (the end of fn_80262648): its
 * player slot and position, from the coin's struct, which only mncharsel.c
 * defines. A coin that is not shown (its slot closed, or no character
 * chosen) is not reported. The next frame reads them. */
void pc_a11y_css_coin(int slot, float x, float y);

/* Once a frame of stage select (mnStageSel_Scene_OnFrame in
 * src/melee/mn/mnstagesel.c), after ranked play's own list and before
 * everything else, the skipped scene's first and only frame included. Its
 * state lives in static variables there, handed over raw: the screen's data,
 * the table of tiles (mnStageSel_803F06D0, a row type only that file
 * defines, and the size of its row), the hovered tile (mnStageSel_804D6CAE),
 * the pick state (mnStageSel_804D6CAF), the frames input is still ignored
 * (mnStageSel_804D6CA4), the buttons the screen read on its last frame
 * (mnStageSel_804D6CA0), the port the stage cursor answers to
 * (mnStageSel_804D50A0, -1 for every port), and online the two players'
 * picks (-1 while unknown). Stage select speech compares the screen with
 * the last frame here. */
struct SSSData;
void pc_a11y_sss_frame(const struct SSSData* data, const void* tiles, int tile_size, int hovered,
    int pick, int frozen, int buttons, int owner, int local_pick, int remote_pick);

/* Stage select is leaving for the match (mnStageSel_Scene_OnFrame): the
 * tile of the stage chosen (mnStageSel_804D6CAE, Random already rolled),
 * and online the two picks it was chosen from, this machine's player's and
 * the other's (net_local_pick, net_remote_pick; -1 offline). Online the
 * coin flip between the picks has just been made here; reached once both
 * picks are known, on a different frame on each machine. */
void pc_a11y_sss_resolved(int stage_tile, int local_pick, int remote_pick);

/* Stage select's cursor has moved this simulated frame (fn_8025A310, after
 * its clamp, before it tests the tiles): where it is, in the units the tiles
 * are tested in, and the stick that moved it, each axis with the dead zone
 * of 30 already taken off (mnStageSel_804D6CAC and 804D6CAD). Offline that
 * stick is the first port's pushed past the dead zone, which may be another
 * player's. Not reached once a pick is made. Steering's glide advances
 * here. */
void pc_a11y_sss_cursor(float x, float y, int stick_x, int stick_y);

/* Once a frame of the online lobby (gm_Scene_OnlineLobby_OnFrame in
 * src/melee/gm/gmonlinemode.c), after the view is drawn, on LAN play's path
 * and on the internet lobbies' (Direct Connect, Unranked, Ranked, Profile):
 * the view the scene filled this frame (OnlineLobbyView, gmonlinemode.h).
 * Lobby speech compares it with the last frame here. */
struct OnlineLobbyView;
void pc_a11y_lobby_frame(const struct OnlineLobbyView* view);

/* A simulation tick has run (gm_RunSimTick in src/melee/gm/gmscene.c, right
 * after the replay recorder's pc_slp_tick_end), in every scene that ticks,
 * menus included: every GObj has updated and the fighters' collisions are
 * settled. held_mask has a bit set for each GObj list the tick held still
 * (1 << HSD_GOBJ_PLINK_FIGHTER for the fighters). The match cues read the
 * match here (docs/a11y/cues.md). Reached again for re-run ticks during a
 * rollback. */
void pc_a11y_match_tick(uint64_t held_mask);

/* A sound effect has a voice of the engine's own (HSD_AudioSFXStartParam's
 * play in src/sysdolphin/baselib/axdriver.c), on the game thread, before it
 * first sounds: voice is the engine's id, never a netplay handle, sound the
 * game's sound id, whose bank says which file it is from (a stage's own,
 * say), and track the game's grouping, which for a fighter's sounds names
 * its slot. The fork may change the voice's volume and pan (0 left, 0x80
 * centre, 0xFF right) to place the sound where its fighter is (positional
 * sound effects) or to mute a stage's own sounds (docs/a11y/cues.md).
 * script_pan is the pan the sound's own script starts each later layer
 * from (the voice's x1C): the engine uses pan for the first layer only.
 * Never reached for a rollback's re-run start. */
void pc_a11y_sfx_started(
    int voice, int sound, int track, uint8_t* volume, uint8_t* pan, uint8_t* script_pan);

/* A sound's own script has set an absolute pan (AXDriverInterp's pan
 * command in src/sysdolphin/baselib/axdriver.c), which the engine applies
 * at once. The pan to use: the script's own, or for a voice the fork placed
 * (pc_a11y_sfx_started), the script's distance from the centre added to
 * the voice's place, so the sound stays where its fighter is. On the
 * engine's audio thread: answers from a lock-free table and nothing else. */
int pc_a11y_sfx_script_pan(int voice, int pan);

/* A fighter's sound is about to start with no track of its own to say
 * whose it is (ft_PlaySFX in src/melee/ft/ft_0877.c, the way most fighter
 * sounds play): slot and sub (Nana) name the fighter, and the next start,
 * if it comes before the next tick, is placed as theirs. */
void pc_a11y_fighter_sfx(int slot, bool sub);

/* A sound object is about to start its sound (lbaudio_ax.c: a fighter's
 * landings and steps from its action script's stage sound command): owner
 * is the object's owner, and the next start, when it is a fighter, is
 * placed as theirs. */
struct HSD_GObj;
void pc_a11y_sound_object_starting(struct HSD_GObj* owner);

/* A hit's sound is about to start (lbColl_80005BB0 in
 * src/melee/lb/lbcollision.c): track is the fighter track its caller asked
 * for, which the start then drops for track 0, or -1 for an item's. The next
 * start, if it comes before the next tick, is placed as if on that track. */
void pc_a11y_sfx_track_hint(int track);

/* A sound object's pan and volume as they go to the engine each frame
 * (lbaudio_ax.c, the objects lbAudioAx_800263E8 makes), so a value placed
 * at the start would be overwritten. owner is the
 * object's owner, a fighter, an item or none. The game's own pan (0 left,
 * 64 centre, 127 right) and volume (0 to 127) are returned, or, for a
 * fighter's object with positional sound effects on, its fighter's place.
 * They change nothing the game keeps. Reached in a rollback's re-run too,
 * where the last tick's places stand. */
int pc_a11y_sound_object_pan(struct HSD_GObj* owner, int pan);
int pc_a11y_sound_object_volume(struct HSD_GObj* owner, int volume);

/* Port 1's virtual pad is about to be published (publish_locked in
 * src/pc/keyboard.c, once per video frame, after the keyboard, the key
 * driver and a GameCube adapter are merged). The one place the fork writes:
 * steering on the screen on now (character select's hand, stage select's
 * cursor) adds its stick to the pad, as a player's controller would, before
 * the game and the netplay code sample it. Returns true when it did, so the
 * pad is published even with no other source active. What it asks for is
 * published once and then dropped, so the stick returns to rest by itself. */
struct PADStatus;
bool pc_a11y_pad(struct PADStatus* pad);

/* Controller 1's physical buttons are about to reach online quick chat
 * (pc_net_chat_poll in src/pc/vi.c, once per video frame). Returns the
 * buttons chat may see: on character and stage select, while the D-pad
 * steers, the D-pad is left out, so steering sends no phrases. Chat is presentation
 * only; the game and the netplay code read the pads elsewhere. */
uint16_t pc_a11y_chat_buttons(uint16_t buttons);

#ifdef __cplusplus
}

/* The Accessibility category of the launcher's Settings and of the port menu
 * (docs/adr/0007-the-accessibility-category-is-wired-into-the-base-ports-menus.md),
 * for the handlers of Launcher and PortMenu in src/pc/launcher.cpp, which are
 * C++ and act on element ids; defined in settings_page.cpp, which builds the
 * category's rows into its empty page as the document loads. Each hand-off
 * returns true when the id is one of the fork's rows and the fork has acted on
 * it, so the base port's handler must then do nothing. */
#include <string>
#include <vector>
namespace Rml {
class ElementDocument;
class Event;
}  // namespace Rml

/* The category's rows' ids in their order, for page_focus. */
std::vector<std::string> pc_a11y_settings_rows();
/* Whether the id is one of the fork's sliders, for is_slider, which the
 * base port's own handlers use to tell a slider from a text field. */
bool pc_a11y_settings_slider(const std::string& id);
/* Enter, A or a click on the element (Launcher::action, PortMenu::apply). */
bool pc_a11y_settings_press(Rml::ElementDocument* document, const std::string& id);
/* Left (-1) or Right (1) on the element (Launcher::horizontal,
 * PortMenu::step), by keys, D-pad or stick. */
bool pc_a11y_settings_step(Rml::ElementDocument* document, const std::string& id, int direction);
/* A Change event on the document: a slider stepped by RmlUi or dragged. */
bool pc_a11y_settings_change(Rml::Event& event);
/* Escape or B in the document, before the page goes back (Launcher::back,
 * PortMenu::back): out of the open submenu of the fork's rows, one level up,
 * onto the row that opens it
 * (docs/adr/0009-the-accessibility-settings-split-into-submenus-inside-the-one-category.md).
 * False on the top level and outside the fork's rows. */
bool pc_a11y_settings_back(Rml::ElementDocument* document);

/* The update check follows the fork's releases, not the base port's
 * (docs/adr/0011-fork-releases-are-versioned-apart-from-the-base-port.md):
 * src/pc/updater.cpp asks the fork's repository and lets fork code pick the
 * release, and src/pc/launcher.cpp shows the fork's version and words where
 * it showed the base port's. Defined in update_hooks.cpp, on the logic in
 * update_check.cpp. */
namespace pc::updater {
struct UpdateState;
}  // namespace pc::updater

/* The release list's path on api.github.com (WinHTTP) and its whole URL
 * (curl). */
const wchar_t* pc_a11y_update_api_path();
const char* pc_a11y_update_api_url();
/* Before the list is fetched: with the developer switch
 * MELEE_A11Y_UPDATE_LIST naming a file, reads the list from it into body
 * and returns true, so the network is never asked. */
bool pc_a11y_update_list_file(std::string& body);
/* The list fetched, under the updater's lock: picks the release to offer and
 * sets the state's status, message, release and zip, in place of the base
 * port's choice. */
void pc_a11y_update_result(const std::string& body, pc::updater::UpdateState& state);
/* The fork's releases page, for the browser when no release is on offer. */
const char* pc_a11y_update_releases_page();
/* The fork's Discord channel, which the launcher's Discord buttons open in
 * place of the base port's server, for every player. */
const char* pc_a11y_discord_url();
/* Where the launcher's title bar and the port menu's show the app version:
 * "0.1.0 (local build, Melee-PC v0.4.2)" (a11y::shown_version). */
std::string pc_a11y_shown_version();
/* The offered release's version, "0.1.1", for the update banner's tag. */
std::string pc_a11y_update_version(const pc::updater::UpdateState& state);
/* "Update available: Non-Visual Melee 0.1.1", for the status lines. */
std::string pc_a11y_update_available(const pc::updater::UpdateState& state);
/* "A newer version of Non-Visual Melee is available (0.1.1).", the banner's
 * description before the base port's instructions. */
std::string pc_a11y_update_description(const pc::updater::UpdateState& state);
/* "Non-Visual Melee is up to date (0.1.0).", for the status lines. */
std::string pc_a11y_update_up_to_date();
#endif
