/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Game translation units get this header force-included; see CMakeLists.txt. */
#include "pc/compat.h"
#include "character_kinds.h"
#include "game_access.h"
#include "menu_kinds.h"
#include "pc/pc.h"
#include "pc/region.h"
#include "scene_kinds.h"
#include <melee/ft/forward.h>
#include <melee/gm/forward.h>
#include <melee/gm/gmmain_lib.h>
#include <melee/mn/forward.h>
#include <melee/mn/mnmain.h>
#include <melee/mn/mnonline.h>
#include <melee/mn/types.h>
#include <melee/pl/forward.h>
#include <string.h>
#include <sysdolphin/baselib/sislib.h>
#include <sysdolphin/baselib/synth.h>

/* The fork's copy of the scene kinds against the decomp's (scene_kinds.h). */
#define A11Y_CHECK_SCENE_KIND(name, number)                                                        \
    _Static_assert(name == (number), #name " no longer has the number scene_kinds.h gives it");
A11Y_SCENE_KINDS(A11Y_CHECK_SCENE_KIND)
#undef A11Y_CHECK_SCENE_KIND

/* The fork's copy of the menu kinds and entries against the decomp's
 * (menu_kinds.h). */
#define A11Y_CHECK_MENU_NUMBER(name, number)                                                       \
    _Static_assert(name == (number), #name " no longer has the number menu_kinds.h gives it");
A11Y_MENU_KINDS(A11Y_CHECK_MENU_NUMBER)
A11Y_MENU_SELECTIONS(A11Y_CHECK_MENU_NUMBER)
#undef A11Y_CHECK_MENU_NUMBER

/* The fork's copy of the character kinds against the decomp's
 * (character_kinds.h). */
#define A11Y_CHECK_CHARACTER_KIND(name, number)                                                    \
    _Static_assert(name == (number), #name " no longer has the number character_kinds.h gives "    \
                                           "it");
A11Y_CHARACTER_KINDS(A11Y_CHECK_CHARACTER_KIND)
#undef A11Y_CHECK_CHARACTER_KIND

/* The match rules before the players' data on character select have
 * bitfields. Laid out as MinGW does by default they take 144 bytes instead of
 * the game's 136 (on a 64-bit host), and every player's character is read
 * from the wrong place; a11y.cmake gives this file the game's layout. */
_Static_assert(sizeof(StartMeleeRules) == 136, "StartMeleeRules is laid out unlike the game's");

/* The main menu tree's table of screens (mnmain.c), declared in no header.
 * It has one row per MenuKind up to MENU_KIND_ONLINE, the base port's last. */
extern MenuKindData mn_803EB6B0[];
enum { A11Y_MENU_TABLE_LEN = MENU_KIND_ONLINE + 1 };

/* The main menu's strings sit in font slot 0 (mnMain_Scene_OnEnter). */
enum { A11Y_MENU_FONT = 0 };

const uint8_t* a11y_game_resolve(uint32_t slot) {
    uintptr_t addr = (uintptr_t)pc_resolve_dp(slot);
    /* 0x02xxxxxx slots name host statics the game registered; the same test
     * as sis_bind in sislib.c. */
    if ((slot & 0xFF000000u) == 0x02000000u) {
        return (const uint8_t*)addr;
    }
    if (addr == 0 || addr - OSBaseAddress >= PC_MEM1_SIZE) {
        return NULL;
    }
    return (const uint8_t*)addr;
}

const char* a11y_game_font_symbol(int font_idx) {
    if (font_idx < 0 || font_idx >= (int)ARRAY_SIZE(HSD_SisLib_804D1124) ||
        HSD_SisLib_804D1124[font_idx] == NULL)
    {
        return NULL;
    }
    return HSD_SisLib_FontSymbol(font_idx);
}

const uint8_t* a11y_game_string(int font_idx, int idx) {
    if (a11y_game_font_symbol(font_idx) == NULL || idx < 0) {
        return NULL;
    }
    /* An on-disc array of 32-bit pointer slots, as sis_bind reads it. */
    const DiscU32* table = (const DiscU32*)HSD_SisLib_804D1124[font_idx];
    return a11y_game_resolve(table[idx].v);
}

/* A string of the main menu's table by the number the game's code uses (NTSC-U
 * numbering), or NULL while font slot 0 holds another table. */
static const uint8_t* menu_string(int number) {
    const char* symbol = a11y_game_font_symbol(A11Y_MENU_FONT);
    if (number < 0 || symbol == NULL || strcmp(symbol, "SIS_MenuData") != 0) {
        return NULL;
    }
    return a11y_game_string(A11Y_MENU_FONT, pc_region_sis_index(symbol, number));
}

void a11y_game_menu_state(int center_text, A11yMenuState* out) {
    MenuKind menu = (MenuKind)mn_804A04F0.cur_menu;
    int hovered = mn_804A04F0.hovered_selection;

    out->menu = menu;
    out->hovered = hovered;
    out->description = NULL;
    out->pc_label = mnOnline_Label(menu, hovered);
    out->pc_description = mnOnline_Description(menu, hovered);
    out->center_text = center_text;
    out->center_text_string = menu_string(center_text);
    /* The synth's mode, 1 for stereo, as lbAudioAx_80024BD0 reads it; that
     * function also writes lbaudio_ax.c's copy of the mode, so it is not
     * called here. */
    out->mono = HSD_SynthGetSoundMode() != 1;
    out->balance = (s8)gmMainLib_8015ED74();
    out->deflicker = gmMainLib_8015F4E8() != 0;
    /* The same precedence as mn_80229A7C, which draws the description line:
     * the base port's text first, then SdMenu's by the table's number. */
    if (out->pc_description != NULL || (int)menu >= A11Y_MENU_TABLE_LEN) {
        return;
    }
    const MenuKindData* row = &mn_803EB6B0[menu];
    if (row->description_indices == NULL || hovered >= row->selection_count) {
        return;
    }
    out->description = menu_string(row->description_indices[hovered]);
}

/* The hand's states and what a holding hand holds, as mnCharSel_CursorThink
 * uses them; the decomp has not named them. */
enum {
    A11Y_CSS_HAND_HOLDING = 1,
    A11Y_CSS_HAND_HIDDEN = 3,
    /* Held things 0 to 3 are the coins of player slots 0 to 3; 4 to 7 their
     * CPU level sliders, 8 to 11 their handicap sliders. */
    A11Y_CSS_HELD_COINS = 4,
};

/* A slot's portrait number as the screen keeps it (0x19 and above for none),
 * or -1. */
static int css_portrait(u8 number) {
    return number < A11Y_CSS_PORTRAITS ? number : -1;
}

void a11y_game_css_state(const CSSData* css, const CSSDoorsData* doors, const CSSIcon* icons,
    int hands, int pending_exit, const A11yCssHandReport reports[A11Y_CSS_SLOTS], int local_port,
    A11yCssState* out) {
    memset(out, 0, sizeof(*out));
    out->hands = hands;
    out->leaving = pending_exit != 0;

    /* The players each slot's character is kept for. In single-player modes
     * slot 0 belongs to the port that started the mode and slot 1 (Training's
     * CPU) to another, as mnCharSel_804D6CF0 and mnCharSel_804D6CF1 hold them
     * (mnCharSel_802640A0). */
    int players[A11Y_CSS_SLOTS] = {0, 1, 2, 3};
    int local_hand = local_port;
    if (hands == 1) {
        int first = (s8)(css->unk_0x0 - 1);
        if (first < 0) {
            first = 0;
        }
        players[0] = first;
        players[1] = first == 0 ? 1 : 0;
        local_hand = 0;
    }
    if (local_hand < 0 || local_hand >= A11Y_CSS_SLOTS) {
        local_hand = 0;
    }
    out->local_slot = local_hand;
    out->local_player = players[local_hand];

    const A11yCssHandReport* report = &reports[local_hand];
    out->hand.present = report->seen && report->state != A11Y_CSS_HAND_HIDDEN;
    out->hand.coin = out->hand.present && report->state == A11Y_CSS_HAND_HOLDING &&
                             report->held >= 0 && report->held < A11Y_CSS_HELD_COINS ?
                         report->held :
                         -1;
    out->hand.x = report->x;
    out->hand.y = report->y;

    for (int i = 0; i < A11Y_CSS_SLOTS; i++) {
        const CSSDoor* door = &doors->doors[i];
        A11yCssSlot* slot = &out->slots[i];
        int ckind = css->vs.start.players[players[i]].ckind;
        slot->kind = door->p_kind == Gm_PKind_Human ? A11Y_CSS_HUMAN :
                     door->p_kind == Gm_PKind_Cpu   ? A11Y_CSS_CPU :
                                                      A11Y_CSS_CLOSED;
        slot->portrait = css_portrait(door->sel_icon);
        slot->over_portrait = css_portrait(door->sel_icon_prev);
        slot->character = ckind >= 0 && ckind < CKind_Playable_Count ? ckind : -1;
        slot->costume = door->costume;
    }

    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        const CSSIcon* icon = &icons[i];
        A11yCssPortrait* portrait = &out->portraits[i];
        portrait->character = icon->char_kind;
        /* The hover test's own condition (mnCharSel_CursorThink). */
        portrait->locked = icon->state < ICONSTATE_TEMP;
        portrait->left = icon->bound_l;
        portrait->right = icon->bound_r;
        portrait->top = icon->bound_u;
        portrait->bottom = icon->bound_d;
    }
}
