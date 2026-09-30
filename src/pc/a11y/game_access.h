/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The fork's reads of game memory that need the decomp's headers, which only
 * C can include. Fork-internal: hooks live in a11y_hooks.h. Everything here
 * only reads. */
#pragma once
#include "character_kinds.h"
#include "css_kinds.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CSSData;
struct CSSDoorsData;
struct CSSIcon;
struct CSSTag;
struct HSD_JObj;

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

/* What the main menu tree shows, read once a frame by the menu reader. */
typedef struct A11yMenuState {
    /* The menu screen (MenuKind) and its hovered entry, from mn_804A04F0. On
     * a leaf screen the entry number means whatever that screen uses it
     * for. */
    int menu;
    int hovered;
    /* The hovered entry's description as game text, from SdMenu; NULL where
     * the screen has none or the base port supplies it. */
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
} A11yMenuState;

/* Fills the snapshot from game memory; center_text is the number the hook
 * last passed, or -1. */
void a11y_game_menu_state(int center_text, A11yMenuState* out);

/* Character select: four player slots and 25 portraits. */
enum { A11Y_CSS_SLOTS = 4, A11Y_CSS_PORTRAITS = 25 };

/* What the frame hook hands over (pc_a11y_css_frame). */
typedef struct A11yCssScreen {
    const struct CSSData* css;
    const struct CSSDoorsData* doors;
    const struct CSSIcon* icons;
    const struct CSSTag* tags;
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
 * A held slider keeps the hand at its knob's height and follows the hand's
 * x over 10 units from origin_x, the hand's x at the slider's lowest end. */
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
    /* Where the coin is, as the coin hook last reported it; coin_seen is false
     * until it has since the scene was entered. A coin at rest drifts a
     * little, away from other coins and from its portrait's edges. Filled by
     * the reader, not read from the screen. */
    bool coin_seen;
    float coin_x;
    float coin_y;
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
    /* The HMN/CPU and team buttons, the sliders' knobs, and the name box
     * (read for the local player's slot only), where the game tests them;
     * VS modes only, and not while the rules screen or name entry opens or
     * is open. */
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
    A11yCssExit exit;
    /* The session is online: other players' changes are spoken. Set by the
     * caller, not read from the screen. */
    bool online;
    /* The local player's slot, and the player number shown for them (0 for
     * "P1"); they differ in single-player modes, where the one slot belongs
     * to whichever port started the mode. */
    int local_slot;
    int local_player;
    A11yCssHand hand;
    /* The player slots the buttons of a VS mode reach: 3 in Camera mode,
     * whose fourth slot is the camera, else 4. */
    int slot_count;
    /* The top bar's Teams button and rules header exist; the handicap rule
     * is on, so each slot shows a handicap slider. */
    bool has_teams_button;
    bool has_rules_button;
    bool handicap_sliders;
    /* A team match. */
    bool teams;
    /* Ready to Fight is shown. */
    bool ready;
    A11yCssSlot slots[A11Y_CSS_SLOTS];
    A11yCssPortrait portraits[A11Y_CSS_PORTRAITS];
} A11yCssState;

/* Fills the snapshot from what the frame hook passed, the hands the hand hook
 * reported, indexed by hand, and the port the local player drives. */
void a11y_game_css_state(const A11yCssScreen* screen,
    const A11yCssHandReport reports[A11Y_CSS_SLOTS], int local_port, A11yCssState* out);

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

#ifdef __cplusplus
}
#endif
