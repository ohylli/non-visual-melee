/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The fork's reads of game memory that need the decomp's headers, which only
 * C can include. Fork-internal: hooks live in a11y_hooks.h. Everything here
 * only reads. */
#pragma once
#include "character_kinds.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CSSData;
struct CSSDoorsData;
struct CSSIcon;

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

/* The local player's hand. */
typedef struct A11yCssHand {
    /* False until the hand has updated once in this scene. A hand whose
     * controller is unplugged stops updating and keeps its last report. */
    bool present;
    /* The player slot whose coin the hand carries, or -1. */
    int coin;
    /* Position in the screen's units: x from -35 to 26, y from -22 to 25. */
    float x;
    float y;
} A11yCssHand;

/* What a player slot's HMN/CPU tab shows. */
typedef enum A11yCssSlotKind {
    A11Y_CSS_HUMAN,
    A11Y_CSS_CPU,
    /* Drawn "N/A". A closed slot shows no coin, but its character may hold a
     * stale value. */
    A11Y_CSS_CLOSED,
} A11yCssSlotKind;

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
} A11yCssSlot;

/* One portrait. */
typedef struct A11yCssPortrait {
    /* The character, as the game's table holds it. */
    A11yCharacter character;
    /* A locked portrait is drawn as "?" or not at all, and never hovered. */
    bool locked;
    /* The rectangle a carried coin hovers it in, in the screen's units. */
    float left;
    float right;
    float top;
    float bottom;
} A11yCssPortrait;

/* What character select shows, read once a frame by its reader. */
typedef struct A11yCssState {
    /* 4 in VS modes, 1 in single-player modes. */
    int hand_count;
    /* The screen has begun to leave, or opened the rules screen or name
     * entry. */
    bool leaving;
    /* The local player's slot, and the player number shown for them (0 for
     * "P1"); they differ in single-player modes, where the one slot belongs
     * to whichever port started the mode. */
    int local_slot;
    int local_player;
    A11yCssHand hand;
    A11yCssSlot slots[A11Y_CSS_SLOTS];
    A11yCssPortrait portraits[A11Y_CSS_PORTRAITS];
} A11yCssState;

/* Fills the snapshot from what the frame hook passed (pc_a11y_css_frame), the
 * hands the hand hook reported, indexed by hand, and the port the local
 * player drives. */
void a11y_game_css_state(const struct CSSData* css, const struct CSSDoorsData* doors,
    const struct CSSIcon* icons, int hand_count, int pending_exit,
    const A11yCssHandReport reports[A11Y_CSS_SLOTS], int local_port, A11yCssState* out);

#ifdef __cplusplus
}
#endif
