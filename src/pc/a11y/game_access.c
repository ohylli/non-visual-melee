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
#include <melee/gm/types.h>
#include <melee/lb/lb_00B0.h>
#include <melee/lb/lbspdisplay.h>
#include <melee/mn/forward.h>
#include <melee/mn/mnmain.h>
#include <melee/mn/mnonline.h>
#include <melee/mn/types.h>
#include <melee/pl/forward.h>
#include <string.h>
#include <sysdolphin/baselib/sislib.h>
#include <sysdolphin/baselib/synth.h>

/* The fork's copies of the decomp's numbers (scene_kinds.h, menu_kinds.h,
 * character_kinds.h) against the decomp's own. */
#define A11Y_CHECK_NUMBER(name, number)                                                            \
    _Static_assert(name == (number), #name " no longer has the number the fork's copy gives it");
A11Y_SCENE_KINDS(A11Y_CHECK_NUMBER)
A11Y_MENU_KINDS(A11Y_CHECK_NUMBER)
A11Y_MENU_SELECTIONS(A11Y_CHECK_NUMBER)
A11Y_CHARACTER_KINDS(A11Y_CHECK_NUMBER)
#undef A11Y_CHECK_NUMBER

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

/* The hand's holding state and what a holding hand holds, as
 * mnCharSel_CursorThink uses them; the decomp has not named them. */
enum {
    A11Y_CSS_HAND_HOLDING = 1,
    /* Held things 0 to 3 are the coins of player slots 0 to 3; 4 to 7 their
     * CPU level sliders, 8 to 11 their handicap sliders. */
    A11Y_CSS_HELD_COINS = 4,
    A11Y_CSS_HELD_CPU_LEVELS = 8,
    A11Y_CSS_HELD_HANDICAPS = 12,
};

/* The handicap rule's settings. On shows a slider per slot; on and auto both
 * move the CPU level slider to its second place. */
enum { A11Y_HANDICAP_OFF = 0, A11Y_HANDICAP_AUTO = 1, A11Y_HANDICAP_ON = 2 };

/* The player slots the buttons reach in Camera mode, whose fourth slot is the
 * camera. */
enum { A11Y_CSS_CAMERA_MODE_SLOTS = 3 };

/* The numbers of mnCharSel_CursorThink's tests, in the screen's units. A
 * slider's grab point is this far from its joint. */
static const float A11Y_CSS_KNOB_OFFSET_X = -2.9f;
static const float A11Y_CSS_KNOB_OFFSET_Y = 1.7f;
/* The top and bottom of the HMN/CPU and team buttons; their sides are the
 * slot's own. */
static const float A11Y_CSS_SLOT_BUTTON_TOP = 0.2f;
static const float A11Y_CSS_SLOT_BUTTON_BOTTOM = -4.6f;
static const float A11Y_CSS_TEAM_BUTTON_TOP = -1.0f;
static const float A11Y_CSS_TEAM_BUTTON_BOTTOM = -5.8f;
/* The name box around its joint. */
static const float A11Y_CSS_NAME_BOX_LEFT = -4.7f;
static const float A11Y_CSS_NAME_BOX_RIGHT = 5.2f;
static const float A11Y_CSS_NAME_BOX_TOP = 2.0f;
static const float A11Y_CSS_NAME_BOX_BOTTOM = -1.0f;

/* A slot's portrait number as the screen keeps it (0x19 and above for none),
 * or -1. */
static int css_portrait(u8 number) {
    return number < A11Y_CSS_PORTRAITS ? number : -1;
}

/* Where a joint of the screen's model is, as mnCharSel_CursorThink finds it
 * for the sliders and the name box. lb_8000B1CC sets up the joint's world
 * matrix if it is out of date: a cache of the model's drawing, which the
 * game's own A presses and every frame's drawing fill with the same values,
 * so filling it here changes nothing the game decides. */
static bool css_joint(HSD_JObj* model_root, u8 joint, Vec3* out) {
    HSD_JObj* jobj = NULL;
    if (model_root == NULL || lb_80011E24(model_root, &jobj, joint, -1) == 0 || jobj == NULL) {
        return false;
    }
    lb_8000B1CC(jobj, NULL, out);
    return true;
}

/* A slider's knob: the grab point mnCharSel_CursorThink tests, offset from
 * the slider's joint. */
static A11yCssKnob css_knob(HSD_JObj* model_root, u8 joint) {
    A11yCssKnob knob = {false, 0.0f, 0.0f};
    Vec3 pos;
    if (css_joint(model_root, joint, &pos)) {
        knob.known = true;
        knob.x = A11Y_CSS_KNOB_OFFSET_X + pos.x;
        knob.y = A11Y_CSS_KNOB_OFFSET_Y + pos.y;
    }
    return knob;
}

static A11yCssRect css_rect(float left, float right, float top, float bottom) {
    A11yCssRect rect = {left, right, top, bottom};
    return rect;
}

static bool css_hand_holds(const A11yCssHandReport* report) {
    return report->seen && report->state == A11Y_CSS_HAND_HOLDING;
}

void a11y_game_css_state(const A11yCssScreen* screen,
    const A11yCssHandReport reports[A11Y_CSS_SLOTS], int local_port, A11yCssState* out) {
    const CSSData* css = screen->css;
    int hand_count = screen->hand_count;
    memset(out, 0, sizeof(*out));
    out->hand_count = hand_count;
    out->exit =
        screen->pending_exit < A11Y_CSS_AWAY ? (A11yCssExit)screen->pending_exit : A11Y_CSS_AWAY;
    out->ready = screen->ready != 0;

    /* The players each slot's character is kept for. In single-player modes
     * slot 0 belongs to the port that started the mode and slot 1 (Training's
     * CPU) to another, as mnCharSel_804D6CF0 and mnCharSel_804D6CF1 hold them
     * (mnCharSel_802640A0). */
    int players[A11Y_CSS_SLOTS] = {0, 1, 2, 3};
    int local_hand = local_port;
    if (hand_count == 1) {
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

    /* The top bar's buttons and the slots the buttons reach, by the match
     * type's tests in mnCharSel_CursorThink; single-player modes have their
     * own. */
    bool vs = hand_count == A11Y_CSS_SLOTS;
    /* Opening the rules screen or name entry frees the screen's model and
     * its name tag windows, and leaving them builds new ones
     * (mnCharSel_802640A0); until then only the static tables are safe to
     * read. */
    bool built = out->exit == A11Y_CSS_STAYING || out->exit == A11Y_CSS_TO_STAGE_SELECT ||
                 out->exit == A11Y_CSS_BACK;
    int handicap_rule = gmMainLib_GetGameRules()->handicap;
    out->slot_count = css->match_type == VS_CAMERA ? A11Y_CSS_CAMERA_MODE_SLOTS : A11Y_CSS_SLOTS;
    out->has_teams_button = vs && css->match_type <= VS_SLOWMO;
    out->has_rules_button = vs && css->match_type != VS_STAMINA;
    out->handicap_sliders = handicap_rule == A11Y_HANDICAP_ON;
    out->teams = css->vs.start.rules.is_teams != 0;

    const A11yCssHandReport* report = &reports[local_hand];
    int held = css_hand_holds(report) ? report->held : -1;
    out->hand.present = report->seen;
    out->hand.coin = held >= 0 && held < A11Y_CSS_HELD_COINS ? held : -1;
    out->hand.slider = A11Y_CSS_NO_SLIDER;
    out->hand.slider_slot = -1;
    if (held >= A11Y_CSS_HELD_COINS && held < A11Y_CSS_HELD_CPU_LEVELS) {
        out->hand.slider = A11Y_CSS_CPU_LEVEL;
        out->hand.slider_slot = held - A11Y_CSS_HELD_COINS;
    } else if (held >= A11Y_CSS_HELD_CPU_LEVELS && held < A11Y_CSS_HELD_HANDICAPS) {
        out->hand.slider = A11Y_CSS_HANDICAP;
        out->hand.slider_slot = held - A11Y_CSS_HELD_CPU_LEVELS;
    }
    out->hand.x = report->x;
    out->hand.y = report->y;

    for (int i = 0; i < A11Y_CSS_SLOTS; i++) {
        const CSSDoor* door = &screen->doors->doors[i];
        const CSSTag* tag = &screen->tags[i];
        int player = players[i];
        A11yCssSlot* slot = &out->slots[i];
        int ckind = css->vs.start.players[player].ckind;
        slot->kind = door->p_kind == Gm_PKind_Human ? A11Y_CSS_HUMAN :
                     door->p_kind == Gm_PKind_Cpu   ? A11Y_CSS_CPU :
                                                      A11Y_CSS_CLOSED;
        slot->portrait = css_portrait(door->sel_icon);
        slot->over_portrait = css_portrait(door->sel_icon_prev);
        slot->character =
            ckind >= 0 && ckind < CKind_Playable_Count ? (A11yCharacter)ckind : A11Y_NO_CHARACTER;
        slot->costume = door->costume;
        for (int hand = 0; hand < A11Y_CSS_SLOTS; hand++) {
            if (css_hand_holds(&reports[hand]) && reports[hand].held == i) {
                slot->carried = true;
            }
        }
        slot->hand_holding = css_hand_holds(&reports[i]);
        slot->team = door->team;
        slot->cpu_level = css->vs.start.players[player].cpu_level;
        slot->handicap = css->vs.start.players[player].handicap;
        slot->cpu_level_held = door->is_hold_cpu_slider != 0;
        slot->handicap_held = door->is_hold_handicap_slider != 0;
        slot->name_tags_open = built && tag->data != NULL && tag->data->state != 0;
        if (!vs || !built) {
            continue;
        }
        /* The bounds of mnCharSel_CursorThink's tests; retail nudges the
         * buttons' heights outward by a hair, which is left out here. */
        slot->slot_button = css_rect(door->togglebtn_left, door->togglebtn_right,
            A11Y_CSS_SLOT_BUTTON_TOP, A11Y_CSS_SLOT_BUTTON_BOTTOM);
        slot->team_button = css_rect(door->teambtn_left, door->teambtn_right,
            A11Y_CSS_TEAM_BUTTON_TOP, A11Y_CSS_TEAM_BUTTON_BOTTOM);
        /* With the handicap rule on or automatic, the CPU level slider moves
         * to its second place and the handicap slider takes the first. */
        slot->cpu_level_knob = css_knob(screen->model_root,
            handicap_rule != A11Y_HANDICAP_OFF ? door->cpuslider2_joint : door->cpuslider_joint);
        slot->handicap_knob = css_knob(screen->model_root, door->cpuslider_joint);
        Vec3 name;
        if (i == local_hand && css_joint(screen->model_root, tag->name_jointl, &name)) {
            slot->name_box =
                css_rect(name.x + A11Y_CSS_NAME_BOX_LEFT, name.x + A11Y_CSS_NAME_BOX_RIGHT,
                    name.y + A11Y_CSS_NAME_BOX_TOP, name.y + A11Y_CSS_NAME_BOX_BOTTOM);
        }
    }

    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        const CSSIcon* icon = &screen->icons[i];
        A11yCssPortrait* portrait = &out->portraits[i];
        portrait->character = (A11yCharacter)icon->char_kind;
        /* The hover test's own condition (mnCharSel_CursorThink). */
        portrait->locked = icon->state < ICONSTATE_TEMP;
        portrait->rect = css_rect(icon->bound_l, icon->bound_r, icon->bound_u, icon->bound_d);
    }
}
