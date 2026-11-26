/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The fork's reads of game memory that need the decomp's headers, which only
 * C can include. Fork-internal: hooks live in a11y_hooks.h. Everything here
 * only reads. */
#pragma once
#include "character_kinds.h"
#include "css_kinds.h"
#include "item_kinds.h"
#include "stage_kinds.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CSSData;
struct CSSDoorsData;
struct CSSDoorsData2;
struct CSSDoorsMisc;
struct CSSIcon;
struct CSSTag;
struct HSD_JObj;
struct OnlineLobbyView;
struct SSSData;

/* A disc pointer as stored in game data, as a host address; NULL when it is
 * null or lands outside the game's memory. */
const uint8_t* a11y_game_resolve(uint32_t slot);

/* The symbol of the string table loaded in a font slot ("SIS_MenuData"), or
 * NULL. */
const char* a11y_game_font_symbol(int font_idx);

/* Entry idx of the string table in a font slot, by the table's own numbering
 * (on PAL discs not the number the NTSC-U code uses; see
 * pc_region_sis_index). NULL for a slot outside the game's memory, which is
 * how the end of the table shows: the table stores no length. */
const uint8_t* a11y_game_string(int font_idx, int idx);

/* The match rules the rules screens change, from GameRules. Custom Rules and
 * Additional Rules copy their values there on the frame a value changes
 * (fn_802309F0, fn_80232F44), so the next poll sees it. */
typedef struct A11yRules {
    /* Mode: time, stock, coin or bonus (A11Y_RULES_MODES). */
    int mode;
    /* Minutes, 1 to 99, or 0 for no limit. */
    int time_limit;
    int stock_count;
    /* 0 off, 1 auto, 2 on. */
    int handicap;
    /* In tenths: 5 to 20, drawn 0.5 to 2.0. */
    int damage_ratio;
    /* StageSelectMode (A11Y_STAGE_SELECT_MODES). */
    int stage_selection;
    /* Minutes, 1 to 99, or 0 for no limit. */
    int stock_time_limit;
    bool friendly_fire;
    bool pause;
    bool score_display;
    /* As stored (GameRules.unk_xc): 0 counts a self-destruct as -1, 1 as 0,
     * 2 as -2. */
    int self_destructs;
} A11yRules;

/* Erase Data's rows, top to bottom: Hidden Characters, Hidden Stages, 1-P
 * High Score, VS. Mode Scores, Trophy Collection, Erase All. */
enum { A11Y_ERASE_ROWS = 6 };

/* Erase Data (mndatadel.c), from its gobj's user data, read only while it is
 * the screen. The game's own state: in the base port the drawn YES and NO
 * highlight does not match the choice. */
typedef struct A11yEraseData {
    /* The hovered row, 0 to 5. */
    int row;
    /* The prompt up: 0 none, 1 "Erase?", 2 "Are you sure?" (Erase All's
     * second). */
    int prompt;
    /* The prompt's choice: Yes, or No, where each prompt opens. */
    bool yes;
    /* The rows erased this visit; reopening the screen clears them, and Erase
     * All sets them all. */
    bool erased[A11Y_ERASE_ROWS];
    /* The hovered row's description and the prompt's text as game text, or
     * NULL. */
    const uint8_t* description;
    const uint8_t* prompt_text;
    /* The prompt's text by its SdMenu number, as log lines name it, or 0. */
    int prompt_number;
} A11yEraseData;

/* Rumble's controller panels, P1 to P4. */
enum { A11Y_RUMBLE_PORTS = 4 };

/* One controller on Rumble, from the screen's gobj (mnvibration.c). */
typedef struct A11yRumblePort {
    /* The controller is plugged in, as the screen last saw it; its panel
     * shows a red cross and no value while it is not. */
    bool connected;
    /* Its place: in the names list, or on its own panel, where each
     * controller opens. Left as it was while the controller is unplugged;
     * plugging it in puts it back on its panel. */
    bool in_names;
    /* Its rumble setting (GetRumbleSettingOfPort). */
    bool rumble;
} A11yRumblePort;

/* Rumble (mnvibration.c), from its gobj's user data, read only while it is
 * the screen; all zero elsewhere. */
typedef struct A11yRumble {
    A11yRumblePort ports[A11Y_RUMBLE_PORTS];
    /* The names in the save (GetNameCount); with none the names list is
     * empty and Right does nothing. */
    int name_count;
    /* The names list's one cursor, shared by every controller in it: its
     * row among the eight names shown (x0[1]) and the first name shown
     * (scroll_offset). */
    int names_row;
    int names_scroll;
    /* The hovered name's name tag slot, names_scroll + names_row, as the
     * game keeps its names in the first slots; -1 with none. */
    int name_slot;
    /* The hovered name, up to eight bytes of Shift-JIS and a terminator;
     * empty with none. */
    char name[9];
    /* The hovered name's rumble setting (its name tag's rumble_enabled). */
    bool name_rumble;
    /* The line the screen always draws at its foot, "Player name entry must
     * precede Rumble setting.", as game text, or NULL. */
    const uint8_t* bottom_line;
} A11yRumble;

/* What the main menu tree shows, read once a frame by the menu reader. */
typedef struct A11yMenuState {
    /* The menu screen (MenuKind) and its hovered entry, from mn_804A04F0. On
     * a leaf screen the entry number means whatever that screen uses it
     * for: the row on Custom Rules and Additional Rules. */
    int menu;
    int hovered;
    /* The hovered entry's description as game text, from SdMenu; NULL where
     * the screen has none or the base port supplies it. On Custom Rules and
     * Additional Rules, the hovered row's, as the screen chooses it for the
     * row's value in rules. */
    const uint8_t* description;
    /* The base port's plain label and description for the hovered entry
     * (mnOnline_Label, mnOnline_Description), or NULL. */
    const char* pc_label;
    const char* pc_description;
    /* The centre text a leaf screen last set, by its NTSC-U SdMenu number
     * (pc_a11y_menu_center_text), or -1 before any; and that text as game
     * text, or NULL. Stale on screens that set none. */
    int center_text;
    const uint8_t* center_text_string;
    /* The settings Sound and Screen display change, as saved at each press:
     * the channel (Mono or Stereo), the balance from -100 (music only) to
     * +100 (sounds only), and the deflicker preference. */
    bool mono;
    int balance;
    bool deflicker;
    A11yRules rules;
    /* The hovered row's value as the screen's input last set it
     * (mn_804A04F0.confirmed_selection). On Item Switch an item's 1 on or 0
     * off, or on the frequency strip the frequency, 0 None to 5 Very High;
     * on Random Stage the stage's 1 on or 0 off. The screens copy it into
     * their own data when it changes, and into GamePrefs' masks only at the
     * next A press or on leaving, so the masks lag for the row last
     * changed. */
    int confirmed;
    /* Random Stage refused to turn off the last stage that was on, since the
     * last snapshot (pc_a11y_stage_switch_refused). Set by the caller, not
     * read from the screen. */
    bool refused;
    /* The game's language as saved (lbLang_GetSavedLanguage): Japanese, or
     * English. Language changes it and reloads the menu scene, whose text is
     * Japanese from then on while it is. */
    bool language_japanese;
    /* Language's cursor (mnlanguage.c) on Japanese, or on English. Its state
     * is static to its file, so its frame hook reports it
     * (pc_a11y_language_frame); before the screen's first report the cursor
     * is where the screen opens it, on the saved language. Meaningless on
     * other screens. */
    bool language_cursor_japanese;
    /* Erase Data's state; all zero on other screens. */
    A11yEraseData erase;
    /* Rumble's state; all zero on other screens. */
    A11yRumble rumble;
} A11yMenuState;

/* Fills the snapshot from game memory, refused left false; center_text is
 * the number the hook last passed, or -1; language_cursor is Language's
 * cursor as its frame hook reported it on the frame before (0 Japanese, 1
 * English), or -1 for no report. */
void a11y_game_menu_state(int center_text, int language_cursor, A11yMenuState* out);

/* Character select: four player slots and 25 portraits. */
enum { A11Y_CSS_SLOTS = 4, A11Y_CSS_PORTRAITS = 25 };

/* What the frame hook hands over (pc_a11y_css_frame). */
typedef struct A11yCssScreen {
    const struct CSSData* css;
    const struct CSSDoorsData* doors;
    const struct CSSIcon* icons;
    const struct CSSTag* tags;
    const struct CSSDoorsMisc* misc;
    const struct CSSDoorsData2* data2;
    struct HSD_JObj* model_root;
    int hand_count;
    int pending_exit;
    int ready;
} A11yCssScreen;

/* One hand as the hand hook reported it, in the raw numbers of mncharsel.c's
 * cursor struct; seen is false until the hook has reported it since the scene
 * was entered. */
typedef struct A11yCssHandReport {
    bool seen;
    int state;
    int held;
    float x;
    float y;
} A11yCssHandReport;

/* One player slot's coin as the coin hook reported it; seen is false until
 * the hook has reported it since the scene was entered. A coin at rest drifts
 * a little, away from other coins and from its portrait's edges. */
typedef struct A11yCssCoinReport {
    bool seen;
    float x;
    float y;
} A11yCssCoinReport;

/* A rectangle in the screen's units, y growing upwards. The game's tests are
 * strict: a point on an edge is outside. */
typedef struct A11yCssRect {
    float left;
    float right;
    float top;
    float bottom;
} A11yCssRect;

/* A slider's knob: the point a free hand grabs it from, within the game's
 * grab distance. known is false where the game's model has no such joint.
 * origin_x is the hand's x at the slider's lowest end; css_targets.cpp has
 * how a held slider's value follows the hand from there. */
typedef struct A11yCssKnob {
    bool known;
    float x;
    float y;
    float origin_x;
} A11yCssKnob;

/* The sliders of a player slot. */
typedef enum A11yCssSlider {
    A11Y_CSS_NO_SLIDER,
    A11Y_CSS_CPU_LEVEL,
    A11Y_CSS_HANDICAP,
} A11yCssSlider;

/* The local player's hand. */
typedef struct A11yCssHand {
    /* False until the hand has updated once in this scene. A hand whose
     * controller is unplugged stops updating and keeps its last report. */
    bool present;
    /* The player slot whose coin the hand carries, or -1. */
    int coin;
    /* The slider the hand holds, and whose; -1 while it holds none. */
    A11yCssSlider slider;
    int slider_slot;
    /* Position in the screen's units: x from -35 to 26, y from -22 to 25. */
    float x;
    float y;
} A11yCssHand;

/* One player slot along the bottom (the decomp's door). */
typedef struct A11yCssSlot {
    A11yCssSlotKind kind;
    /* The portrait the slot's coin marks (sel_icon), or -1 for none. Picking
     * the coin up sets a placeholder (0xD) here, which stays until the coin
     * is first over a portrait, so the portrait under a carried coin is
     * over_portrait. */
    int portrait;
    /* The portrait a carried coin was last found over (sel_icon_prev), or -1
     * while it is over none. At rest it equals portrait. */
    int over_portrait;
    /* The chosen character, or none. Picking the coin up keeps it; carrying
     * the coin down into the player slots clears it. */
    A11yCharacter character;
    /* The costume's number, from 0. */
    int costume;
    /* Some hand carries the slot's coin. */
    bool carried;
    /* Where the coin is. */
    A11yCssCoinReport coin;
    /* The slot's own hand (the hand of the same port) holds a coin or a
     * slider; its HMN/CPU button does not react meanwhile. */
    bool hand_holding;
    /* The team in a team match: 0 red, 1 blue, 2 green. */
    int team;
    /* The sliders' values, 1 to 9, and whether some hand holds each. */
    int cpu_level;
    int handicap;
    bool cpu_level_held;
    bool handicap_held;
    /* The slot's name tag window is open. */
    bool name_tags_open;
    /* The HMN/CPU and team buttons and the sliders' knobs, VS modes only,
     * and the name box, read for the local player's slot only, where the
     * game tests them; not while the rules screen or name entry opens or is
     * open. */
    A11yCssRect slot_button;
    A11yCssRect team_button;
    A11yCssKnob cpu_level_knob;
    A11yCssKnob handicap_knob;
    A11yCssRect name_box;
} A11yCssSlot;

/* One portrait. */
typedef struct A11yCssPortrait {
    /* The character, as the game's table holds it. */
    A11yCharacter character;
    /* A locked portrait is drawn as "?" or not at all, and never hovered. */
    bool locked;
    /* The rectangle a carried coin hovers it in. */
    A11yCssRect rect;
} A11yCssPortrait;

/* A row of a single-player mode's arrows, below the portraits: A on the
 * lower arrow steps the row's value one down, on the higher one up, and at
 * that end of the range does nothing. */
typedef struct A11yCssArrowRow {
    bool shown;
    /* Stale where the row is not shown. */
    int value;
    /* The ends of the range. */
    int lowest;
    int highest;
    /* Where the game tests the hand for each arrow. */
    A11yCssRect lower;
    A11yCssRect higher;
} A11yCssArrowRow;

/* The single-player modes' arrows: the difficulty, drawn "LEVEL", 0 (very
 * easy) to 4 (very hard), in Classic, Adventure and All-Star, and below it
 * the stock count, 1 to 5, in Classic and Adventure. */
typedef struct A11yCssArrows {
    A11yCssArrowRow difficulty;
    A11yCssArrowRow stocks;
} A11yCssArrows;

enum {
    A11Y_CSS_LOWEST_DIFFICULTY = 0,
    A11Y_CSS_HIGHEST_DIFFICULTY = 4,
    A11Y_CSS_FEWEST_STOCKS = 1,
    A11Y_CSS_MOST_STOCKS = 5,
};

/* Where the screen is going: mncharsel.c's pending exit, by its numbers. */
typedef enum A11yCssExit {
    A11Y_CSS_STAYING = 0,
    /* Start was pressed with Ready to Fight shown; the next frame may still
     * refuse and stay. */
    A11Y_CSS_TO_STAGE_SELECT = 1,
    A11Y_CSS_BACK = 2,
    /* The rules screen or name entry is asked for, and opens next frame
     * inside this scene. */
    A11Y_CSS_TO_RULES = 3,
    A11Y_CSS_TO_NAME_ENTRY = 4,
    /* The rules screen or name entry is open. Leaving it builds the screen
     * afresh, with the hands at home, as if arriving. */
    A11Y_CSS_AWAY = 5,
} A11yCssExit;

/* What character select shows, read once a frame by its reader. */
typedef struct A11yCssState {
    /* 4 in VS modes, 1 in single-player modes. */
    int hand_count;
    /* The mode: the screen's CSSMatchType, an A11yCssMatchType in
     * single-player modes. */
    int match_type;
    A11yCssExit exit;
    /* The session is online: other players' changes are spoken. Set by the
     * caller, not read from the screen. */
    bool online;
    /* The local player's slot, and the player number shown for them (0 for
     * "P1"); they differ in single-player modes, where the one slot belongs
     * to whichever port started the mode. */
    int local_slot;
    int local_player;
    /* The controller port whose pad moves the local hand: the local player's
     * in VS modes, the port that started a single-player mode. */
    int local_port;
    /* A was pressed on that pad this frame; the hand acts on it as it
     * updates, after this snapshot. */
    bool pressed_a;
    A11yCssHand hand;
    /* The player slots steps and speech go through: in VS modes those the
     * buttons reach, 4, or 3 in Camera mode, whose fourth slot is the
     * camera; in single-player modes the mode's own, 1, or 2 in Training,
     * the second the CPU's, and the slots after those read as closed. */
    int slot_count;
    /* The top bar's Teams button and rules header exist; the handicap rule
     * is on, so each slot shows a handicap slider. */
    bool has_teams_button;
    bool has_rules_button;
    bool handicap_sliders;
    /* A team match. */
    bool teams;
    /* The match rules, which the rules header shows. */
    A11yRules rules;
    /* Ready to Fight is shown. */
    bool ready;
    A11yCssArrows arrows;
    A11yCssSlot slots[A11Y_CSS_SLOTS];
    A11yCssPortrait portraits[A11Y_CSS_PORTRAITS];
} A11yCssState;

/* Fills the snapshot from what the frame hook passed, the hands the hand hook
 * reported, indexed by hand, the coins the coin hook reported, indexed by
 * slot, and the port the local player drives. */
void a11y_game_css_state(const A11yCssScreen* screen,
    const A11yCssHandReport reports[A11Y_CSS_SLOTS], const A11yCssCoinReport coins[A11Y_CSS_SLOTS],
    int local_port, A11yCssState* out);

/* A controller as the game read it this simulated frame (HSD_PadCopyStatus):
 * the main stick after the game's clamping, and the D-pad directions newly
 * pressed. Online, a port's pad is its player's synced input. */
typedef struct A11yPad {
    int stick_x;
    int stick_y;
    bool pressed_left;
    bool pressed_right;
    bool pressed_up;
    bool pressed_down;
} A11yPad;

void a11y_game_pad(int port, A11yPad* out);

/* Stage select: 30 stage tiles in mnstagesel.c's table, the last the random
 * tile; the hovered index is 30 until the stage cursor first enters one. */
enum { A11Y_SSS_TILES = 30, A11Y_SSS_RANDOM_TILE = 29, A11Y_SSS_NO_TILE = 30 };

/* What the frame hook hands over (pc_a11y_sss_frame), in mnstagesel.c's raw
 * numbers. */
typedef struct A11ySssScreen {
    const struct SSSData* data;
    /* The table of tiles, whose row type only mnstagesel.c defines, and the
     * size of a row there, which game_access.c checks against its copy. */
    const void* tiles;
    int tile_size;
    int hovered;
    int pick;
    int frozen;
    int buttons;
    int owner;
    int local_pick;
    int remote_pick;
} A11ySssScreen;

/* How a tile is drawn, by the table's own numbers. */
typedef enum A11ySssTileState {
    /* A locked stage outside the stacked pairs: not drawn, never hovered. */
    A11Y_SSS_NOT_DRAWN = 0,
    /* A locked tile, drawn as "?": hovered, but a pick is refused. */
    A11Y_SSS_LOCKED = 1,
    A11Y_SSS_UNLOCKED = 2,
} A11ySssTileState;

/* One stage tile. */
typedef struct A11ySssTile {
    /* The stage, or none for the random tile. */
    A11yStage stage;
    A11ySssTileState state;
    /* The centre of its hit box in the screen's units, y growing upwards,
     * and the box's half extents. The game's test is strict: a point on an
     * edge is outside. */
    float x;
    float y;
    float half_width;
    float half_height;
} A11ySssTile;

/* Where the screen is in picking, by mnStageSel_804D6CAF's numbers. */
typedef enum A11ySssPick {
    A11Y_SSS_CHOOSING = 0,
    /* A stage or Random was picked; the preview shows and the stage cursor
     * is hidden. Offline, Random is rolled at once and the hovered index is
     * the stage rolled. */
    A11Y_SSS_PICKED = 1,
    /* The preview has shown: the match loads, online once both players'
     * picks are known. */
    A11Y_SSS_LEAVING = 2,
} A11ySssPick;

/* What stage select shows, read once a frame by its reader. */
typedef struct A11ySssState {
    /* The stage-select rule Random or Ordered chose the stage beforehand:
     * the screen was never built and leaves on this frame. Nothing below is
     * read but skipped_stage. */
    bool skipped;
    A11yStage skipped_stage;
    /* The session is online. Set by the caller, not read from the screen. */
    bool online;
    /* The port whose controller moves the stage cursor, or -1 for every
     * controller (the first stick pushed past the dead zone moves it). The
     * Turns and Loser rules hand it to one player; online it is the local
     * player's. */
    int owner;
    /* The tile the stage cursor is on, or was on last: crossing empty space
     * keeps it. A11Y_SSS_NO_TILE until the cursor first enters one. */
    int hovered;
    A11ySssPick pick;
    /* A or Start was pressed on the screen's last frame and reached the
     * pick, which acts on the tile hovered before that frame's cursor
     * movement: the hovered index of the previous snapshot. Never set during
     * the screen's first 20 frames, nor once a pick is made. */
    bool pressed_a;
    bool pressed_start;
    /* Online: the tiles each player sent (A11Y_SSS_RANDOM_TILE or
     * A11Y_SSS_NO_TILE for Random), or -1 while unknown. */
    int local_pick;
    int remote_pick;
    A11ySssTile tiles[A11Y_SSS_TILES];
} A11ySssState;

/* Fills the snapshot from what the frame hook passed. False, with the
 * snapshot left skipped, when the table's rows no longer have the layout
 * game_access.c knows. */
bool a11y_game_sss_state(const A11ySssScreen* screen, A11ySssState* out);

/* The online lobby: the view its scene fills each frame (OnlineLobbyView,
 * src/melee/gm/gmonlinemode.h), by the numbers and text the screen shows. */
enum { A11Y_LOBBY_ROWS = 7, A11Y_LOBBY_NAME_LEN = 16, A11Y_LOBBY_MESSAGE_LEN = 96 };

typedef enum A11yLobbyPhase {
    A11Y_LOBBY_SEARCHING,
    A11Y_LOBBY_FOUND,
    /* Waiting for the host, or connecting to it. */
    A11Y_LOBBY_CONNECTING,
    A11Y_LOBBY_STARTING,
    A11Y_LOBBY_FAILED,
} A11yLobbyPhase;

/* Another machine the lobby lists. */
typedef struct A11yLobbyRow {
    char name[A11Y_LOBBY_NAME_LEN];
    /* Another protocol, version string or disc: listed, never paired. */
    bool other_version;
} A11yLobbyRow;

/* Direct Connect's screens, by what the view shows. */
typedef enum A11yDirectScreen {
    /* The hub: a list of choices with a cursor. */
    A11Y_DIRECT_HUB,
    /* The code keyboard. */
    A11Y_DIRECT_KEYS,
    /* The call: a status line, searching, connecting, starting or failed. */
    A11Y_DIRECT_CALL,
} A11yDirectScreen;

enum {
    A11Y_DIRECT_ROWS = 8,
    A11Y_DIRECT_KEYS_LEN = 32,
    A11Y_DIRECT_KEY_COLS = 8,
    A11Y_DIRECT_TITLE_LEN = 32,
};

/* Direct Connect's view, its lines as shown. */
typedef struct A11yDirectState {
    A11yDirectScreen screen;
    char title[A11Y_DIRECT_TITLE_LEN];
    /* "YOU RYAN#K3X-Q2M-7A": your own connect code. */
    char subtitle[A11Y_LOBBY_MESSAGE_LEN];
    /* HUB: the rows and the hovered one. KEYS: rows[0] is the entry line,
     * "Friend's code   # K3X-Q2_-__", and row_count 1. CALL: none. */
    int row_count;
    char rows[A11Y_DIRECT_ROWS][A11Y_LOBBY_MESSAGE_LEN];
    int cursor;
    /* KEYS: the keys row-major, A11Y_DIRECT_KEY_COLS to a row, and the
     * hovered one. */
    char keys[A11Y_DIRECT_KEYS_LEN + 1];
    int key_cursor;
    /* The button line the view names, "" where it names none (the screen
     * then draws its own, mnOnlineLobby_Update). */
    char hint[A11Y_LOBBY_MESSAGE_LEN];
    /* CALL: milliseconds since the call began searching, 0 before and once
     * it stops (pc_net_match_progress). */
    unsigned call_ms;
    /* CALL, starting: the countdown line above the status line, "Starting
     * in 1...", as drawn; "" otherwise. */
    char countdown[A11Y_DIRECT_TITLE_LEN];
} A11yDirectState;

typedef struct A11yLobbyState {
    /* LAN play's list of machines. Every other layout is false and not read
     * further here. */
    bool lan;
    /* Direct Connect's hub, code keyboard or call, read into direct. False
     * on a frame whose view shows none of them, and for MELEE_LAN_DIRECT, a
     * developer fixture that routes Direct Connect through LAN play's path
     * and stays unread. */
    bool direct_connect;
    /* The lobby's phase; Direct Connect's hub and code keyboard always
     * show the found one. */
    A11yLobbyPhase phase;
    /* The other machines, this one left out, in the order shown. */
    int row_count;
    A11yLobbyRow rows[A11Y_LOBBY_ROWS];
    /* The status line, as shown. */
    char message[A11Y_LOBBY_MESSAGE_LEN];
    A11yDirectState direct;
} A11yLobbyState;

/* From the frame's view; reached from LAN play's path and from the internet
 * lobbies' (Direct Connect, Unranked, Ranked, Profile). */
void a11y_game_lobby_state(const struct OnlineLobbyView* view, A11yLobbyState* out);

/* True when the online mode was entered for LAN play. */
bool a11y_game_lan_play(void);

/* True when the online mode was entered for Direct Connect over the
 * internet, MELEE_LAN_DIRECT's LAN route left out. */
bool a11y_game_direct_connect(void);

/* The Training menu's rows, by the cursor's numbers: Speed to Camera hold a
 * value, Reset and Finish none, and the hidden row is never reached (the
 * cursor skips it) nor drawn. */
enum {
    A11Y_TRAINING_SPEED = 0,
    A11Y_TRAINING_ITEM = 1,
    A11Y_TRAINING_CPU_COUNT = 2,
    A11Y_TRAINING_CPU_BEHAVIOUR = 3,
    A11Y_TRAINING_CPU_DAMAGE = 4,
    A11Y_TRAINING_HIDDEN = 5,
    A11Y_TRAINING_CAMERA = 6,
    A11Y_TRAINING_RESET = 7,
    A11Y_TRAINING_FINISH = 8,
};

/* The slots in the menu's value array (menu_values), one per row from Speed
 * to Camera, the hidden row's included: a count, not a row. */
enum { A11Y_TRAINING_VALUE_SLOTS = 7 };

/* The Training menu, read once a tick in the Training scene by its reader
 * (.scratch/training-menu/spec.md), from gm_80473814 (gm_1884.c). The scene's
 * start puts the cursor on Speed and every value at its default; both then
 * persist between openings until the scene is left. Meaningless outside the
 * Training scene: the struct keeps whatever the last session left. */
typedef struct A11yTrainingMenu {
    /* The menu is open (x01). The menu's own update sets it on the first
     * tick it runs with the Training pause on, the same tick it starts
     * reading the controller, and clears it on the tick the pause ends,
     * applying the values; so it changes once per Start. */
    bool open;
    /* The row the cursor is on (x00), A11Y_TRAINING_SPEED to
     * A11Y_TRAINING_FINISH. */
    int row;
    /* The row's value (menu_values[row]) on a row with one, else 0. Speed is
     * an index into the speeds, fastest first; Item an index into the
     * Training menu's own item table (gmTraining_ItemTable); the number of
     * CPUs one less than drawn; the CPU's behaviour and the camera an index
     * into their words; the CPU's damage the percent itself. */
    int value;
} A11yTrainingMenu;

/* Fills the snapshot from game memory. Reads only. */
void a11y_game_training_menu(A11yTrainingMenu* out);

/* Results (GS_RESULTS), read once a tick by its reader
 * (.scratch/results/spec.md) from lbl_8046DBE8 and the MatchEnd it points to
 * (gmresult.c, gmresultplayer.c). Slot i is port i; the screen shows four. */
enum { A11Y_RESULTS_PORTS = 4 };

/* The scene's state (lbl_8046DBE8.x1), by the scene's numbers. */
typedef enum A11yResultsState {
    /* The announcer names the winner on a frame count; no press counts,
     * but on a cancelled match a human's Start skips the wait. */
    A11Y_RESULTS_ANNOUNCING = 0,
    /* The summary table animates in. */
    A11Y_RESULTS_SLIDING = 1,
    /* The summary table is up: the first state where a press counts, and
     * any press from a connected human opens every stats panel. */
    A11Y_RESULTS_SUMMARY = 2,
    /* The stats panels, each worked by its port's controller. */
    A11Y_RESULTS_STATS = 3,
    /* Every panel is ready: a short delay, then the scene leaves. */
    A11Y_RESULTS_LEAVING = 4,
} A11yResultsState;

/* MatchKind, by the decomp's numbers (checked in game_access.c). */
typedef enum A11yResultsMatchKind {
    A11Y_RESULTS_TIME = 0,
    A11Y_RESULTS_STOCK = 1,
    A11Y_RESULTS_COIN = 2,
    A11Y_RESULTS_BONUS = 3,
} A11yResultsMatchKind;

/* A port's slot. A demo slot, which no results screen should meet, reads as
 * a CPU. */
typedef enum A11yResultsSlot {
    A11Y_RESULTS_EMPTY = 0,
    A11Y_RESULTS_HUMAN = 1,
    A11Y_RESULTS_CPU = 2,
} A11yResultsSlot;

/* The stats panel's pages, by the scene's numbers. */
typedef enum A11yResultsPage {
    A11Y_RESULTS_PAGE_KOS = 0,
    A11Y_RESULTS_PAGE_STATS = 1,
    A11Y_RESULTS_PAGE_BONUSES = 2,
} A11yResultsPage;

/* The Stats page's rows, one per stat (each drawn as a label row and a value
 * row below it), and the most Bonuses rows the snapshot keeps. */
enum { A11Y_RESULTS_STAT_COUNT = 23, A11Y_RESULTS_MAX_BONUSES = 64 };

/* One row of the KOs and falls page's two lists: another player, the KOs
 * the panel's player dealt them and the times they KO'd the panel's
 * player. */
typedef struct A11yResultsOpponent {
    int port;
    /* The opponent's name tag where the screen shows it, Shift-JIS, copied
     * with a terminator the game's eight bytes may lack; empty where it
     * shows the port ("P2"). */
    char tag[9];
    int kos;
    int falls;
} A11yResultsOpponent;

/* One row of the Bonuses page. */
typedef struct A11yResultsBonus {
    /* The bonus's number in the game's table (fn_8016F1F0). */
    int kind;
    /* Its name as game text (SdRst), or NULL. */
    const uint8_t* name;
    int points;
} A11yResultsBonus;

/* One port's column of the summary table, as the screen shows it: the
 * numbers already clamped as the screen clamps them, and the rows of its
 * stats panel. Zero for an empty port. */
typedef struct A11yResultsPlayer {
    A11yResultsSlot slot;
    /* The character the player ended as: Zelda or Sheik by the form they
     * were in at the end, as the results screen's model and the announcer
     * have it. */
    A11yCharacter character;
    /* The team in a team match: 0 red, 1 blue, 2 green. */
    int team;
    /* The place image, from 0 for 1st; tied players share it. -1 on a
     * cancelled match, which shows none. */
    int place;
    /* The game's order among players (is_small_loser): unique where places
     * tie, 0 for the winner. */
    int rank;
    /* The KOs, Falls and SDs rows. */
    int kos;
    int falls;
    int sds;
    /* Stock: the screen shows the time the player was knocked out, in whole
     * seconds; false where it shows "-:-" (a survivor, the winner, or the
     * winning team's players still in or knocked out on the last frame). */
    bool knocked_out;
    int ko_seconds;
    /* Time: the total, -9999 to 9999. Bonus: the points. */
    int score;
    /* Coin: the coins. */
    int coins;

    /* The stats panel: its page, and whether the player is ready (closed
     * the panel with Start). CPUs and empty ports are ready from the
     * panels' opening. */
    A11yResultsPage page;
    bool ready;
    /* The port's controller is unplugged (its pad reports an error): the
     * game readies a human there as soon as the panels take input, as it
     * does a second human offline, whose port has no controller. Online
     * both players' ports read as plugged in. */
    bool unplugged;
    /* The KOs and falls page: every other player, by port, and the SDs
     * row's count. */
    A11yResultsOpponent opponents[A11Y_RESULTS_PORTS - 1];
    int opponent_count;
    int page_sds;
    /* The Stats page's values in the rows' order; a negative one is drawn
     * as "---". */
    int stats[A11Y_RESULTS_STAT_COUNT];
    /* The Bonuses page, in the order drawn. */
    A11yResultsBonus bonuses[A11Y_RESULTS_MAX_BONUSES];
    int bonus_count;
} A11yResultsPlayer;

typedef struct A11yResults {
    /* The scene has its MatchEnd; nothing below is filled otherwise. */
    bool valid;
    A11yResultsState state;
    A11yResultsMatchKind match_kind;
    bool teams;
    /* No Contest or Retry: no places, no Bonuses page. */
    bool cancelled;
    /* The port the winner's banner names (x6), and the winning team (x5) in
     * a team match. Meaningless on a cancelled match. */
    int winner;
    int winning_team;
    /* The port the local player plays on: controller 1's offline, the local
     * player's online. Set by the caller, not read from the screen. */
    int local_port;
    /* A netplay session: the other human is your opponent. Set by the
     * caller. */
    bool online;
    /* The match type's title as game text (SdRst), or NULL. */
    const uint8_t* match_type_text;
    /* The KOs and falls page's headings as game text (SdRst), or NULL:
     * "KOs", "Falls" and "SDs". */
    const uint8_t* kos_heading;
    const uint8_t* falls_heading;
    const uint8_t* sds_heading;
    /* The Stats page's labels as game text (SdRst), the same for every
     * player: one string per screen row, so a label drawn over two rows
     * ("Attempted", "Grabs") has a second, NULL otherwise. */
    const uint8_t* stat_labels[A11Y_RESULTS_STAT_COUNT][2];
    /* The buttons the local player's controller newly pressed this tick that
     * the reading cursor uses: the local port's pad, online its player's
     * synced input. */
    bool pressed_up;
    bool pressed_down;
    bool pressed_left;
    bool pressed_right;
    bool pressed_z;
    /* Any bit the local player's controller newly set this tick, stick
     * directions included: the game's own test for opening the panels, so
     * on their first tick it says whether the press that opened them was
     * yours. */
    bool pressed_any;
    A11yResultsPlayer players[A11Y_RESULTS_PORTS];
} A11yResults;

/* Fills the snapshot from game memory. Meaningless outside the results
 * scene, where lbl_8046DBE8 keeps whatever the last visit left. Reads
 * only. */
void a11y_game_results(int local_port, A11yResults* out);

/* The memory card scene's message window keeps its strings (SdMsgBox) in
 * this font slot (gm_801AEBB0 in gm_1ADD.c loads them there). */
enum { A11Y_MEMCARD_FONT = 3 };

/* A string of the memory card scene's message window by the number the
 * game's code uses, or NULL while its font slot holds another table. */
const uint8_t* a11y_game_memcard_string(int number);

/* The unlock notice scene keeps its strings (SdPrize) in this font slot
 * (ifprize.c loads them there). */
enum { A11Y_PRIZE_FONT = 2 };

/* A string of the unlock notice scene's table by the number the game's code
 * uses, or NULL while its font slot holds another table. A trophy notice
 * composes its text into string 0x4A at run time. */
const uint8_t* a11y_game_prize_string(int number);

/* The New challenger screen's challenger and whom it challenges, from the
 * challenger data the mode filled before the screen (gm_InitChallengerData,
 * gm_1736.c), which the screen itself reads (gm_ModeState_Approach_OnEnter). */
typedef struct A11yChallenger {
    /* The challenger; A11Y_NO_CHARACTER for a kind beyond the playable
     * ones, whose number is in kind. */
    A11yCharacter character;
    int kind;
    /* The challenged human's controller port, whose A or Start alone
     * starts the fight. */
    int port;
    /* The challenged human's name tag, Shift-JIS with a terminator; empty
     * for none. */
    char tag[9];
    /* Humans in the match that brought the challenger: the VS match's
     * standings in VS and its special melees, 1 in a single-player mode. */
    int humans;
} A11yChallenger;

/* Fills the snapshot. Meaningful on the New challenger screen alone, where
 * the challenger data is the screen's; elsewhere it holds whatever the last
 * challenger left. Reads only. */
void a11y_game_challenger(A11yChallenger* out);

/* A match, read once a tick by the match cues (docs/a11y/cues.md, "The
 * match snapshot"), in world units: x grows to the right, y upwards. */

/* One player slot's main fighter: the slot's active fighter, so Nana and
 * the sleeping form of Zelda or Sheik never count. */
typedef struct A11yMatchFighter {
    int slot;
    /* At the fighter's feet (cur_pos). */
    float x;
    float y;
    bool airborne;
    /* Knocked out, or between death and respawn (motion states 0 to 11). On
     * the respawn platform it is back in play. */
    bool out;
    /* Its input is on: from GO, when the game enables every fighter's. */
    bool can_move;
    /* Its team in a team match, as the slot holds it. */
    int team;
} A11yMatchFighter;

/* Where on the stage, read fresh each tick because some stages move it. */
typedef struct A11yStageFrame {
    /* Outside these a fighter is knocked out (the top one only in some
     * states). */
    float blast_left;
    float blast_right;
    float blast_top;
    float blast_bottom;
    /* The area the camera may show, well inside the blast zones. */
    float camera_left;
    float camera_right;
    float camera_top;
    float camera_bottom;
} A11yStageFrame;

/* An end of a floor's surface: its line followed through the floor lines
 * joined to it, as mpFloorGetLeft and mpFloorGetRight follow them. */
typedef struct A11yFloorEnd {
    float x;
    float y;
    /* A fall from it would cross the bottom blast zone: no wall rises from
     * it, and the game's own cast finds no floor straight down from just
     * above it, the edge offset outwards. A platform's end over the main
     * floor is not deadly. */
    bool deadly;
} A11yFloorEnd;

/* A floor below a fighter, platforms included. */
typedef struct A11yFloorBelow {
    bool found;
    /* The floor line, a valid id whenever found; -1 otherwise. */
    int line;
    /* The floor's height straight below the fighter. On the ground the
     * feet's height; in the air the cast's hit, which can lie a few units
     * above the feet (see A11yMatchState.below), so the drop to it can come
     * out negative. */
    float y;
    /* Its surface's ends, filled whenever found. A fighter in the air can
     * be up to a unit past an end, where the cast still meets the floor
     * (the game lets a line reach a unit further where another joins it). */
    A11yFloorEnd left;
    A11yFloorEnd right;
} A11yFloorBelow;

/* Opponents the snapshot keeps: the nearest one for now. */
enum { A11Y_MATCH_OPPONENTS = 1 };

/* Items the snapshot keeps, nearest to you first: twice a Party Ball's
 * burst of up to 15, made on one tick. The game lets 40 common items and
 * 100 Food lie about at once, so only several Party Balls of Food could cut
 * the list short. */
enum { A11Y_MATCH_ITEMS = 32 };

/* An item in the fork's sense (.scratch/items/spec.md, "Which items"): on
 * the game's item list, of a common kind, and not made by a fighter, so
 * character pulls, Pokémon and projectiles are never one. */
typedef struct A11yMatchItem {
    /* The game's serial (Item.x1C), counted up from 0 each match for every
     * item made, projectiles included: it names one item as long as it
     * lives and is never given again within a match. Container contents are
     * new items with new serials. Online a rollback re-runs the counter
     * (it_804D6D10) from where the window began, so an item made inside it
     * can take another serial on the corrected timeline, and a serial seen
     * can name another item. */
    int serial;
    A11yItem kind;
    /* Item.pos, which is not the item's bottom: an item resting on a floor
     * sits about 2 to 7 units above it. */
    float x;
    float y;
    bool airborne;
    /* Lying about for anyone to take: grabbable (Item_IsGrabbable), not
     * held, and not flying after a throw. A thrown item stays thrown
     * (Item.xDC8_word.flags.x14) through its bounces and off a wall, and is
     * loose again once it comes to rest, when the game puts it into its
     * state on the ground and clears the flag. A new item falling in is
     * loose. So are the Starman and the Mushrooms, never grabbable since
     * walking into them, not a press, takes them. */
    bool loose;
    /* The game's own test says pressing the button would take it now
     * (ftpickupitem_800942A0): in the box in front of you, so facing away
     * puts it out of reach. On the ground A picks it up, light or heavy,
     * and while you hold an item only a healing one is in reach; in the air
     * Z catches it, a light item only, and nothing is in reach while you
     * hold one, as Z then throws it. The test's single nearest answer, so
     * at most one item has it, and none while you are out. */
    bool within_reach;
    /* The slot whose fighter holds it (Item.xDC8_word.flags.x13), -1 for
     * none: lying about, flying after a throw, or held by no fighter. */
    int holder;
} A11yMatchItem;

typedef struct A11yMatchState {
    /* A match with your fighter in it is on, and the stage's frame is
     * known. Nothing below is filled otherwise. */
    bool running;
    /* The game holds the match still on purpose: a player paused (VS or
     * Training), the GAME! freeze, a single-player mode's score tally, an
     * event's scripted freeze. A slow-motion mode's skipped ticks are not
     * paused; they only hold the fighters. */
    bool paused;
    /* GAME! or TIME! has come (A11yMatchHud.decided): the match is over,
     * though its last ticks run on. Training never comes to it. */
    bool decided;
    /* The fighters' updates did not run this tick: paused, or a skipped
     * tick of slow motion. */
    bool fighters_held;
    /* The player the match cues follow, as character select does: offline
     * the human slot on controller 1, else the first human slot (a
     * single-player mode started from another controller); online the
     * human slot on pc_net_local_player()'s port. */
    A11yMatchFighter you;
    /* The floor below you while you are in play. On the ground, the floor
     * under your feet (coll_data.floor.index). In the air, the first floor
     * the game's own cast (mpCheckFloor) meets straight down from just
     * above the bottom of your collision box, which sits at or above your
     * feet, to the bottom blast zone, skipping a platform you are dropping
     * through; none in the air means a gap, where falling straight down
     * would cross the bottom blast zone (on the ground none means only an
     * unreadable floor). Not found while you are out. Its ends, deadly or
     * not, are the ledge warning's. */
    A11yFloorBelow below;
    /* The nearest other slot's main fighter in play, not on your team in a
     * team match, by straight-line distance; none when nobody is in play. */
    int opponent_count;
    A11yMatchFighter opponents[A11Y_MATCH_OPPONENTS];
    A11yStageFrame stage;
    /* The items, nearest to you first by straight-line distance from your
     * feet, filled in paused ticks too (Training's item menu brings an item
     * in while it holds the match). item_count are listed; items_in_match is
     * how many there were, more only when the list was cut short. */
    int item_count;
    int items_in_match;
    A11yMatchItem items[A11Y_MATCH_ITEMS];
} A11yMatchState;

/* Fills the snapshot after a simulated tick (pc_a11y_match_tick), and from
 * nowhere else: its casts are only safe between ticks (ADR-0006).
 * local_port is the controller port the local player plays on; held_mask is
 * the tick's mask of GObj lists held still; edge_offset is how far past
 * each end of the floor below you the deadly test casts (A11yFloorEnd).
 * Reads only. */
void a11y_game_match_state(
    int local_port, bool online, uint64_t held_mask, float edge_offset, A11yMatchState* out);

/* What the match's HUD shows of each player, and what each takes, read once
 * a tick by the match announcements (.scratch/match-announcements/spec.md;
 * primer docs/a11y/gameplay-state.md, "The HUD"). Slot i is player slot i;
 * in VS and online slot n plays on port n. */
enum { A11Y_MATCH_SLOTS = 6 };

/* A fighter's hands: the item in hand (Fighter.item_gobj), a healing item
 * taken while holding another (x1978), and the worn Bunny Hood (x197C). */
enum { A11Y_MATCH_HANDS = 3 };

/* An item in the fork's sense (A11yMatchItem) in a hand, or none. */
typedef struct A11yMatchHeld {
    /* Its serial (A11yMatchItem.serial); -1 for an empty hand or anything
     * else held, such as a turnip or a Pokémon's ball a fighter made. */
    int serial;
    A11yItem kind;
} A11yMatchHeld;

typedef struct A11yMatchHudPlayer {
    /* The slot is in the match: not empty, and its fighter is on the live
     * fighter list. Nothing below is filled otherwise. */
    bool present;
    /* Its HUD panel shows: the HUD made one for the slot
     * (ifStatus_HudInfo), as it does for the first rules.x0_3 slots in
     * play, Multi-Man's wireframes included. Kept through a KO and the
     * respawn; gone once an eliminated player's panel is taken down. */
    bool panel;
    /* A human; otherwise a computer player (a CPU, a demo player or a
     * boss). */
    bool human;
    /* The controller port. */
    int port;
    /* Its team in a team match, as the slot holds it. */
    int team;
    /* The damage the HUD shows, a whole number (Player_GetDamage). */
    int damage;
    /* Stamina mode: the HUD shows the HP left instead of the damage. */
    bool stamina;
    /* Stocks left. */
    int stocks;
    /* Knocked out, or between death and respawn (motion states 0 to 11,
     * as A11yMatchFighter.out): the KO has reset, or is about to reset, the
     * damage to 0. */
    bool out;
    /* The name tag drawn over the fighter, Shift-JIS, copied with a
     * terminator the game's eight bytes may lack; empty for none. Each
     * copy of the game reads it from its own memory card, so online the
     * peer's is not reliable. */
    char tag[9];
    /* The main fighter's hands. A take puts a serial into one: a pickup, a
     * catch, the same item picked up again after a throw. A serial moving
     * between hands (the Bunny Hood from item_gobj to x197C as it is put
     * on) or kept through Zelda's and Sheik's change is no take. */
    A11yMatchHeld hands[A11Y_MATCH_HANDS];
    /* The game's tally of items taken, by kind (pl_8003E2CC, for the
     * results' Items page): raised by one when the player first holds an
     * item, and when they touch a Starman or a Mushroom, never for an item a
     * fighter made, and not again for a re-pickup of the same item (its
     * Item.xDC8_word.flags.xF marks who held it). It catches the takes the
     * hands miss: a touch item never enters a hand, and an air catch (Z)
     * of a healing item, a Metal Box or a Cloaking Device uses it up within
     * the tick. Game state, so a rollback can lower it. */
    int taken[A11Y_ITEM_COMMON_KINDS];
} A11yMatchHudPlayer;

typedef struct A11yMatchHud {
    /* A match is on: some slot's fighter is on the live fighter list,
     * which is empty outside a match, and not the title screen's attract
     * loop (GM_OPENING_MV: its demo fights and the how-to-play movie). */
    bool match;
    /* VsSceneState.hud_enabled: set as GO ends; cleared again while the
     * pause camera or a single-player mode's scripted scene hides the HUD,
     * and at the match's end. */
    bool hud_enabled;
    /* GAME! or TIME! has come: VsSceneState.unk_0 past 0 (1 from GAME! or
     * TIME!, 2 a single-player mode's score tally, 3 finished), where 0 is
     * the match running, paused included. Training never leaves 0, nor does
     * a match left from the pause. */
    bool decided;
    bool teams;
    /* The HUD shows stocks: a stock match (gm_8016B094), or a mode counting
     * lives (rules.x4_2, gm_8016B0E8: Classic, Adventure, Multi-Man), as
     * the panels' stock icons (ifStatus_802F6508) and the stock loss at a
     * KO (ftCo_800D34E0) decide. */
    bool shows_stocks;
    /* The match has a timer (StartMeleeRules.timer_enabled), and it counts
     * up (Multi-Man, All-Star) rather than down. */
    bool timer;
    bool timer_counts_up;
    /* VsSceneState.timer_seconds: on a countdown the whole seconds left; -1
     * for a countdown with no limit, which no mode in the game starts. */
    int timer_seconds;
    /* VsSceneState.unk_2C, the ticks within the second, 0 to 59: a countdown
     * starts at 59 and shows "N:00.00" there, then on the next tick takes a
     * second off and shows "N-1:59.99" at 0. */
    int timer_frames;
    /* The local player's slot, as A11yMatchState.you: -1 for none (four
     * computer players). */
    int you;
    /* A netplay session. Set by the caller. */
    bool online;
    A11yMatchHudPlayer players[A11Y_MATCH_SLOTS];
} A11yMatchHud;

/* Fills the snapshot after a simulated tick (pc_a11y_match_tick), in any
 * scene: outside a match it says so. local_port is the controller port the
 * local player plays on. Reads only. */
void a11y_game_match_hud(int local_port, bool online, A11yMatchHud* out);

/* Positional sound effects (docs/a11y/cues.md, "Positional sound effects"):
 * where each slot's fighters stand, as the game files their sounds by
 * track. Index 0 is the slot's main fighter, 1 its sub fighter (Nana). */
typedef struct A11ySfxFighter {
    bool present;
    /* At the fighter's feet (cur_pos). */
    float x;
    float y;
} A11ySfxFighter;

typedef struct A11ySfxSources {
    A11ySfxFighter fighters[A11Y_MATCH_SLOTS][2];
} A11ySfxSources;

/* Fills the fighters after a simulated tick; none outside a match. Reads
 * only. */
void a11y_game_sfx_sources(A11ySfxSources* out);

/* A playing sound's pan, by the engine's own voice id as
 * pc_a11y_sfx_started gave it (0 left, 0x80 centre, 0xFF right). Sound
 * engine state only, never the simulation's. False once the voice has
 * ended or been reused. */
bool a11y_game_sfx_set_pan(int voice, uint8_t pan);
/* Its volume likewise, in the engine's units (0 to 0xFF). */
bool a11y_game_sfx_set_volume(int voice, uint8_t volume);

/* The sound bank a sound id is from, by its index in the game's bank list
 * (lbaudio_ax.c's ssm_files: 0 main.ssm, 35 corneria.ssm), 55 for none. */
int a11y_game_sfx_bank(int sound);

/* The fighter a sound object belongs to (pc_a11y_sound_object_pan's
 * owner): its slot and whether it is the sub fighter (Nana); false for an
 * owner that is no fighter, or none. Reads only. */
struct HSD_GObj;
bool a11y_game_sound_owner(const struct HSD_GObj* owner, int* slot, bool* sub);

#ifdef __cplusplus
}
#endif
