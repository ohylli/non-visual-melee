/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The fork's reads of game memory that need the decomp's headers, which only
 * C can include. Fork-internal: hooks live in a11y_hooks.h. Everything here
 * only reads. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
} A11yMenuState;

void a11y_game_menu_state(A11yMenuState* out);

#ifdef __cplusplus
}
#endif
