/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Game translation units get this header force-included; see CMakeLists.txt. */
#include "pc/compat.h"
#include "game_access.h"
#include "menu_kinds.h"
#include "pc/pc.h"
#include "pc/region.h"
#include "scene_kinds.h"
#include <melee/gm/forward.h>
#include <melee/gm/gmmain_lib.h>
#include <melee/mn/forward.h>
#include <melee/mn/mnmain.h>
#include <melee/mn/mnonline.h>
#include <melee/mn/types.h>
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
