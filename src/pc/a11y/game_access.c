/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Game translation units get this header force-included; see CMakeLists.txt. */
#include "pc/compat.h"
#include "character_kinds.h"
#include "game_access.h"
#include "item_kinds.h"
#include "menu_kinds.h"
#include "pc/pc.h"
#include "pc/region.h"
#include "scene_kinds.h"
#include "stage_kinds.h"
#include <melee/ft/forward.h>
#include <melee/ft/inlines.h>
#include <melee/ft/kinds/ftCommon/forward.h>
#include <melee/ft/types.h>
#include <melee/gm/forward.h>
#include <melee/gm/gm_1884.h>
#include <melee/gm/gmmain_lib.h>
#include <melee/gm/gmonlinemode.h>
#include <melee/gm/gmscene.h>
#include <melee/gm/gmvs.h>
#include <melee/gm/types.h>
#include <melee/gr/forward.h>
#include <melee/gr/stage.h>
#include <melee/it/forward.h>
#include <melee/lb/lb_00B0.h>
#include <melee/lb/lbspdisplay.h>
#include <melee/mn/forward.h>
#include <melee/mn/mnmain.h>
#include <melee/mn/mnonline.h>
#include <melee/mn/types.h>
#include <melee/mp/mplib.h>
#include <melee/pl/forward.h>
#include <melee/pl/player.h>
#include <string.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/sislib.h>
#include <sysdolphin/baselib/synth.h>

/* The fork's copies of the decomp's numbers (scene_kinds.h, menu_kinds.h,
 * character_kinds.h, stage_kinds.h, item_kinds.h) against the decomp's own. */
#define A11Y_CHECK_NUMBER(name, number)                                                            \
    _Static_assert(name == (number), #name " no longer has the number the fork's copy gives it");
A11Y_SCENE_KINDS(A11Y_CHECK_NUMBER)
A11Y_MENU_KINDS(A11Y_CHECK_NUMBER)
A11Y_MENU_SELECTIONS(A11Y_CHECK_NUMBER)
A11Y_RULES_MODES(A11Y_CHECK_NUMBER)
A11Y_STAGE_SELECT_MODES(A11Y_CHECK_NUMBER)
A11Y_CHARACTER_KINDS(A11Y_CHECK_NUMBER)
A11Y_CSS_MATCH_TYPES(A11Y_CHECK_NUMBER)
A11Y_STAGE_KINDS(A11Y_CHECK_NUMBER)
A11Y_ITEM_KINDS(A11Y_CHECK_NUMBER)
#undef A11Y_CHECK_NUMBER
_Static_assert(A11Y_CSS_VS_CAMERA == VS_CAMERA, "VS_CAMERA no longer has the fork's number");

/* The match rules before the players' data on character select have
 * bitfields. Laid out as MinGW does by default they take 144 bytes instead of
 * the game's 136 (on a 64-bit host), and every player's character is read
 * from the wrong place; a11y.cmake gives this file the game's layout. */
_Static_assert(sizeof(StartMeleeRules) == 136, "StartMeleeRules is laid out unlike the game's");

/* The main menu tree's table of screens (mnmain.c), declared in no header.
 * It has one row per MenuKind up to MENU_KIND_ONLINE, the base port's last. */
extern MenuKindData mn_803EB6B0[];
enum { A11Y_MENU_TABLE_LEN = MENU_KIND_ONLINE + 1 };

/* Custom Rules' description numbers (mnmainrule.c), declared in no header:
 * one per row, and on the Rules, Handicap and Stage Selection rows one per
 * value, the value's at the byte its number gives from x0. The stock count's
 * is its own. */
struct mn_803EC818_t {
    u8 x0;
    u8 pad_1[4];
};
extern struct mn_803EC818_t mn_803EC818[7];
extern u8 mn_StockCountTextId;

/* Additional Rules' value bounds and description numbers (mnruleplus.c), as
 * that file declares the table. */
extern struct {
    u8 stat[6][2];
    u8 desc[6][3];
    u8 pad[2];
} mn_803ED2E8;

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
 * numbering), or NULL while font slot 0 holds another table. Character
 * select loads its own table there (SdSlChr) and runs the rules screens
 * inside itself; they draw the same numbers from it, which hold the same
 * text at every number they use (docs/a11y/native-menus.md, "The rules
 * screens on character select"). PAL remaps each table its own way. */
static const uint8_t* menu_string(int number) {
    const char* symbol = a11y_game_font_symbol(A11Y_MENU_FONT);
    if (number < 0 || symbol == NULL ||
        (strcmp(symbol, "SIS_MenuData") != 0 && strcmp(symbol, "SIS_SelCharData") != 0))
    {
        return NULL;
    }
    return a11y_game_string(A11Y_MENU_FONT, pc_region_sis_index(symbol, number));
}

static void rules_state(A11yRules* out) {
    const GameRules* rules = gmMainLib_GetGameRules();
    out->mode = rules->mode;
    out->time_limit = rules->time_limit;
    out->stock_count = rules->stock_count;
    out->handicap = rules->handicap;
    out->damage_ratio = rules->damage_ratio;
    out->stage_selection = rules->stage_sel;
    out->stock_time_limit = rules->stock_time_limit;
    out->friendly_fire = rules->friendly_fire != 0;
    out->pause = rules->pause != 0;
    out->score_display = rules->score_display != 0;
    out->self_destructs = rules->unk_xc;
}

/* A Custom Rules row's description for the row's value, as mn_802307F8
 * chooses it; NULL for a row or value outside its table. */
static const uint8_t* custom_rules_description(int row, const GameRules* rules) {
    if (row < 0 || row >= (int)ARRAY_SIZE(mn_803EC818)) {
        return NULL;
    }
    const struct mn_803EC818_t* numbers = &mn_803EC818[row];
    int value;
    switch (row) {
    case 0:
        value = rules->mode;
        break;
    case 1:
        return menu_string(rules->mode == Mode_Stock ? mn_StockCountTextId : numbers->x0);
    case 2:
        value = rules->handicap;
        break;
    case 4:
        value = rules->stage_sel;
        break;
    default:
        return menu_string(numbers->x0);
    }
    if (value == 0) {
        return menu_string(numbers->x0);
    }
    if (value > (int)ARRAY_SIZE(numbers->pad_1)) {
        return NULL;
    }
    return menu_string(numbers->pad_1[value - 1]);
}

/* An Additional Rules row's description for the row's value, as
 * mnRulePlus_GetDescIdx chooses it; NULL for a row or value outside its
 * table. */
static const uint8_t* additional_rules_description(int row, const GameRules* rules) {
    if (row < 0 || row >= (int)ARRAY_SIZE(mn_803ED2E8.desc)) {
        return NULL;
    }
    int value;
    switch (row) {
    case 1:
        value = rules->friendly_fire;
        break;
    case 2:
        value = rules->pause;
        break;
    case 3:
        value = rules->score_display;
        break;
    case 4:
        value = rules->unk_xc;
        break;
    default:
        value = 0;
        break;
    }
    if (value >= (int)ARRAY_SIZE(mn_803ED2E8.desc[row])) {
        return NULL;
    }
    return menu_string(mn_803ED2E8.desc[row][value]);
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
    rules_state(&out->rules);
    out->confirmed = mn_804A04F0.confirmed_selection;
    out->refused = false;
    /* The same precedence as mn_80229A7C, which draws the description line:
     * the base port's text first, then SdMenu's by the table's number. */
    if (out->pc_description != NULL || (int)menu >= A11Y_MENU_TABLE_LEN) {
        return;
    }
    /* The rules screens draw their own description line. */
    if (menu == MENU_KIND_RULES) {
        out->description = custom_rules_description(hovered, gmMainLib_GetGameRules());
        return;
    }
    if (menu == MENU_KIND_RULES_EXTRA) {
        out->description = additional_rules_description(hovered, gmMainLib_GetGameRules());
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
 * camera; the slots a single-player mode has, the player's, and in Training
 * the CPU's too. */
enum {
    A11Y_CSS_CAMERA_MODE_SLOTS = 3,
    A11Y_CSS_SINGLE_PLAYER_SLOTS = 1,
    A11Y_CSS_TRAINING_SLOTS = 2,
};

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
static HSD_JObj* css_joint(HSD_JObj* model_root, u8 joint, Vec3* out) {
    HSD_JObj* jobj = NULL;
    if (model_root == NULL || lb_80011E24(model_root, &jobj, joint, -1) == 0 || jobj == NULL) {
        return NULL;
    }
    lb_8000B1CC(jobj, NULL, out);
    return jobj;
}

/* A slider's knob: the grab point mnCharSel_CursorThink tests, offset from
 * the slider's joint. The joint's own x is how far along the slider the knob
 * is (updateGrabbedSlider), so the knob less it is the lowest end. */
static A11yCssKnob css_knob(HSD_JObj* model_root, u8 joint) {
    A11yCssKnob knob = {false, 0.0f, 0.0f, 0.0f};
    Vec3 pos;
    HSD_JObj* jobj = css_joint(model_root, joint, &pos);
    if (jobj != NULL) {
        knob.known = true;
        knob.x = A11Y_CSS_KNOB_OFFSET_X + pos.x;
        knob.y = A11Y_CSS_KNOB_OFFSET_Y + pos.y;
        knob.origin_x = knob.x - HSD_JObjGetTranslationX(jobj);
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

/* The single-player modes' arrows, by mnCharSel_CursorThink's tests: the
 * stock arrows below match type 0xD (All-Star), the difficulty arrows up to
 * it. */
static A11yCssArrows css_arrows(const CSSData* css, bool vs, const struct CSSDoorsMisc* misc,
    const struct CSSDoorsData2* data2) {
    A11yCssArrows arrows;
    arrows.difficulty = (A11yCssArrowRow){
        .shown = !vs && css->match_type <= REG_ALLSTAR,
        .value = misc->cpu_level,
        .lowest = A11Y_CSS_LOWEST_DIFFICULTY,
        .highest = A11Y_CSS_HIGHEST_DIFFICULTY,
        .lower =
            css_rect(misc->cpudown_left, misc->cpudown_right, misc->cpubtn_top, misc->cpubtn_btm),
        .higher = css_rect(misc->cpuup_left, misc->cpuup_right, misc->cpubtn_top, misc->cpubtn_btm),
    };
    arrows.stocks = (A11yCssArrowRow){
        .shown = !vs && css->match_type < REG_ALLSTAR,
        .value = data2->stocks,
        .lowest = A11Y_CSS_FEWEST_STOCKS,
        .highest = A11Y_CSS_MOST_STOCKS,
        .lower = css_rect(data2->xf8, data2->xfc, data2->x108, data2->x10c),
        .higher = css_rect(data2->x100, data2->x104, data2->x108, data2->x10c),
    };
    return arrows;
}

void a11y_game_css_state(const A11yCssScreen* screen,
    const A11yCssHandReport reports[A11Y_CSS_SLOTS], const A11yCssCoinReport coins[A11Y_CSS_SLOTS],
    int local_port, A11yCssState* out) {
    const CSSData* css = screen->css;
    int hand_count = screen->hand_count;
    memset(out, 0, sizeof(*out));
    out->hand_count = hand_count;
    out->match_type = css->match_type;
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
        if (first < 0 || first >= A11Y_CSS_SLOTS) {
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
    /* A VS mode's hand reads its own port; the one hand of a single-player
     * mode reads the port that started it. Online, a port's pad is its
     * player's synced input. */
    out->local_port = players[local_hand];
    out->pressed_a = (HSD_PadCopyStatus[out->local_port].trigger & HSD_PAD_A) != 0;

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
    /* The slots the mode has: in single-player modes the rest keep whatever
     * the last VS mode left, name tag windows freed since included. */
    int mode_slots = vs                               ? A11Y_CSS_SLOTS :
                     css->match_type == TRAINING_MODE ? A11Y_CSS_TRAINING_SLOTS :
                                                        A11Y_CSS_SINGLE_PLAYER_SLOTS;
    out->slot_count = !vs                          ? mode_slots :
                      css->match_type == VS_CAMERA ? A11Y_CSS_CAMERA_MODE_SLOTS :
                                                     A11Y_CSS_SLOTS;
    out->has_teams_button = vs && css->match_type <= VS_SLOWMO;
    out->has_rules_button = vs && css->match_type != VS_STAMINA;
    rules_state(&out->rules);
    out->handicap_sliders = vs && handicap_rule == A11Y_HANDICAP_ON;
    out->teams = vs && css->vs.start.rules.is_teams != 0;
    out->arrows = css_arrows(css, vs, screen->misc, screen->data2);

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
        if (i >= mode_slots) {
            slot->kind = A11Y_CSS_CLOSED;
            slot->portrait = -1;
            slot->over_portrait = -1;
            slot->character = A11Y_NO_CHARACTER;
            continue;
        }
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
        slot->coin = coins[i];
        slot->team = door->team;
        slot->cpu_level = css->vs.start.players[player].cpu_level;
        slot->handicap = css->vs.start.players[player].handicap;
        slot->cpu_level_held = door->is_hold_cpu_slider != 0;
        slot->handicap_held = door->is_hold_handicap_slider != 0;
        slot->name_tags_open = built && tag->data != NULL && tag->data->state != 0;
        if (!built) {
            continue;
        }
        /* The one name box of a single-player mode has a joint of its own. */
        Vec3 name;
        u8 name_joint = vs ? tag->name_jointl : screen->misc->tag_box_joint;
        if (i == local_hand && css_joint(screen->model_root, name_joint, &name) != NULL) {
            slot->name_box =
                css_rect(name.x + A11Y_CSS_NAME_BOX_LEFT, name.x + A11Y_CSS_NAME_BOX_RIGHT,
                    name.y + A11Y_CSS_NAME_BOX_TOP, name.y + A11Y_CSS_NAME_BOX_BOTTOM);
        }
        if (!vs) {
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

void a11y_game_pad(int port, A11yPad* out) {
    memset(out, 0, sizeof(*out));
    if (port < 0 || port >= A11Y_CSS_SLOTS) {
        return;
    }
    const HSD_PadStatus* pad = &HSD_PadCopyStatus[port];
    out->stick_x = pad->stickX;
    out->stick_y = pad->stickY;
    out->pressed_left = (pad->trigger & HSD_PAD_DPADLEFT) != 0;
    out->pressed_right = (pad->trigger & HSD_PAD_DPADRIGHT) != 0;
    out->pressed_up = (pad->trigger & HSD_PAD_DPADUP) != 0;
    out->pressed_down = (pad->trigger & HSD_PAD_DPADDOWN) != 0;
}

/* A row of stage select's table of tiles (mnStageSel_803F06D0), whose type
 * mnstagesel.c keeps to itself; the hook passes the size of its own row,
 * which a11y_game_sss_state checks against this copy's. */
typedef struct A11ySssTileRow {
    HSD_JObj* model;
    int x4;
    /* An A11ySssTileState. */
    u8 state;
    u8 x9;
    u8 xA;
    u8 stkind;
    f32 half_width;
    f32 half_height;
    f32 x14;
    f32 x18;
} A11ySssTileRow;

static A11yStage sss_stage(int stkind) {
    switch (stkind) {
#define A11Y_STAGE_CASE(name, number)                                                              \
    case number:                                                                                   \
        return A11Y_##name;
        A11Y_STAGE_KINDS(A11Y_STAGE_CASE)
#undef A11Y_STAGE_CASE
    default:
        return A11Y_NO_STAGE;
    }
}

bool a11y_game_sss_state(const A11ySssScreen* screen, A11ySssState* out) {
    const SSSData* data = screen->data;
    memset(out, 0, sizeof(*out));
    /* The skipped scene never ran mnStageSel_Scene_OnEnter, so the table
     * still points at the models of the last stage select, long freed. */
    if (data->force_stage_id >= 0) {
        out->skipped = true;
        out->skipped_stage = sss_stage(data->force_stage_id);
        return true;
    }
    if (screen->tile_size != (int)sizeof(A11ySssTileRow)) {
        out->skipped = true;
        out->skipped_stage = A11Y_NO_STAGE;
        return false;
    }
    out->owner = screen->owner;
    out->hovered = screen->hovered < A11Y_SSS_NO_TILE ? screen->hovered : A11Y_SSS_NO_TILE;
    out->pick = screen->pick < A11Y_SSS_LEAVING ? (A11ySssPick)screen->pick : A11Y_SSS_LEAVING;
    /* The buttons and the input freeze are those the pick saw: the screen
     * reads the buttons and counts the freeze down before its objects
     * update, and the hook runs before the next frame's read. A pick sets
     * the freeze again. */
    bool taking = screen->frozen == 0 && out->pick == A11Y_SSS_CHOOSING;
    out->pressed_a = taking && (screen->buttons & HSD_PAD_A) != 0;
    out->pressed_start = taking && (screen->buttons & HSD_PAD_START) != 0;
    out->local_pick = screen->local_pick;
    out->remote_pick = screen->remote_pick;

    const char* rows = (const char*)screen->tiles;
    for (int i = 0; i < A11Y_SSS_TILES; i++) {
        /* Copied, since the game's row is of its own type, not this one. */
        A11ySssTileRow copy;
        memcpy(&copy, rows + (size_t)i * sizeof(copy), sizeof(copy));
        const A11ySssTileRow* row = &copy;
        A11ySssTile* tile = &out->tiles[i];
        tile->stage = i == A11Y_SSS_RANDOM_TILE ? A11Y_NO_STAGE : sss_stage(row->stkind);
        tile->state =
            row->state <= A11Y_SSS_UNLOCKED ? (A11ySssTileState)row->state : A11Y_SSS_UNLOCKED;
        tile->half_width = row->half_width;
        tile->half_height = row->half_height;
        /* Where fn_8025A310 tests the stage cursor against, read the same
         * way; lb_8000B1CC fills the joint's cached world matrix as the
         * game's own test and drawing do each frame. */
        if (tile->state != A11Y_SSS_NOT_DRAWN && row->model != NULL) {
            Vec3 pos;
            lb_8000B1CC(row->model, NULL, &pos);
            tile->x = pos.x;
            tile->y = pos.y;
        }
    }
    return true;
}

_Static_assert(
    A11Y_LOBBY_ROWS == ONLINE_LOBBY_MAX_PLAYERS - 1, "the lobby lists another number of rows");
_Static_assert(
    A11Y_LOBBY_NAME_LEN == ONLINE_LOBBY_NAME_LEN, "the lobby's names have another length");
_Static_assert(
    A11Y_LOBBY_MESSAGE_LEN == ONLINE_LOBBY_MSG_LEN, "the lobby's status line has another length");

void a11y_game_lobby_state(const OnlineLobbyView* view, A11yLobbyState* out) {
    memset(out, 0, sizeof(*out));
    out->lan = gmOnline_GetKind() == ONLINE_KIND_LAN && view->screen == LOBBY_SCREEN_PLAYERS;
    if (!out->lan) {
        return;
    }
    switch (view->phase) {
    case LOBBY_PHASE_SEARCHING:
        out->phase = A11Y_LOBBY_SEARCHING;
        break;
    case LOBBY_PHASE_FOUND:
        out->phase = A11Y_LOBBY_FOUND;
        break;
    case LOBBY_PHASE_CONNECTING:
        out->phase = A11Y_LOBBY_CONNECTING;
        break;
    case LOBBY_PHASE_STARTING:
        out->phase = A11Y_LOBBY_STARTING;
        break;
    case LOBBY_PHASE_ERROR:
    default:
        out->phase = A11Y_LOBBY_FAILED;
        break;
    }
    /* Row 0 is this machine. */
    for (int i = 1; i < view->player_count && i < ONLINE_LOBBY_MAX_PLAYERS; i++) {
        A11yLobbyRow* row = &out->rows[out->row_count++];
        memcpy(row->name, view->players[i].name, sizeof(row->name));
        row->name[sizeof(row->name) - 1] = '\0';
        row->other_version = view->players[i].incompatible;
    }
    memcpy(out->message, view->message, sizeof(out->message));
    out->message[sizeof(out->message) - 1] = '\0';
}

bool a11y_game_lan_play(void) {
    return gmOnline_GetKind() == ONLINE_KIND_LAN;
}

_Static_assert(A11Y_TRAINING_VALUE_ROWS == ARRAY_SIZE(gm_80473814.menu_values),
    "the Training menu keeps another number of values");

void a11y_game_training_menu(A11yTrainingMenu* out) {
    memset(out, 0, sizeof(*out));
    const CssSubStruct* menu = &gm_80473814;
    out->open = menu->x01 != 0;
    out->row = menu->x00;
    if (out->row < A11Y_TRAINING_VALUE_ROWS) {
        out->value = menu->menu_values[out->row];
    }
}

/* The slot's main fighter, or NULL. The player slots are static and
 * survive every scene, so the fighter must also be on the live fighter
 * list, which is empty outside a match: a slot's pointer that outlived its
 * fighter is never followed. */
static Fighter* match_fighter(int slot) {
    Gm_PKind kind = Player_GetPlayerSlotType(slot);
    if (kind == Gm_PKind_NA) {
        return NULL;
    }
    HSD_GObj* entity = Player_GetEntity(slot);
    if (entity == NULL) {
        return NULL;
    }
    for (HSD_GObj* g = HSD_GObjPLinkHead[HSD_GOBJ_PLINK_FIGHTER]; g != NULL; g = g->next) {
        if (g == entity) {
            return g->classifier == HSD_GOBJ_CLASS_FIGHTER ? GET_FIGHTER(g) : NULL;
        }
    }
    return NULL;
}

static void match_fighter_state(int slot, const Fighter* fp, A11yMatchFighter* out) {
    out->slot = slot;
    out->x = fp->cur_pos.x;
    out->y = fp->cur_pos.y;
    out->airborne = fp->ground_or_air == GA_Air;
    out->out = fp->motion_id >= ftCo_MS_DeadDown && fp->motion_id <= ftCo_MS_Sleep;
    out->can_move = !fp->input_disabled;
    out->team = Player_GetTeam(slot);
}

/* The main floor (A11yStageFrame): false when no enabled floor line
 * crosses x. A solid floor wins over a platform, which can be jumped and
 * dropped through: Yoshi's Story's cloud passes under the stage's middle.
 * Reads the lines between ticks, which changes nothing. */
static bool match_main_floor(float x, A11yStageFrame* out) {
    int line_count = mpLib_8004D164()->line_count;
    int best = -1;
    bool best_solid = false;
    float best_y = 0.0F;
    for (int id = 0; id < line_count; id++) {
        if (mpLineGetKind(id) != CollLine_Floor || !mpLib_80054ED8(id)) {
            continue;
        }
        Vec3 a;
        Vec3 b;
        mpLineGetV0Pos(id, &a);
        mpLineGetV1Pos(id, &b);
        if (x < a.x || x > b.x || a.x >= b.x) {
            continue;
        }
        float y = a.y + (b.y - a.y) * (x - a.x) / (b.x - a.x);
        bool solid = !(mpLineGetFlags(id) & LINE_FLAG_PLATFORM);
        if (best < 0 || (solid && !best_solid) || (solid == best_solid && y < best_y)) {
            best = id;
            best_solid = solid;
            best_y = y;
        }
    }
    if (best < 0) {
        return false;
    }
    Vec3 end;
    mpFloorGetLeft(best, &end);
    out->floor_left = end.x;
    mpFloorGetRight(best, &end);
    out->floor_right = end.x;
    out->floor_y = best_y;
    return true;
}

/* How far above a point a downward cast starts, so that a floor at the
 * point's own height is still met. */
static const float A11Y_CAST_LIFT = 1.0F;

/* The game's own downward cast from (x, top) to bottom, the first floor it
 * meets, platforms included, but not the line skip (-1 for none). The CPU
 * players use it to judge their recoveries. Called between ticks it leaves
 * no trace: it marks the joints outside the cast's box as too far and
 * clears every mark before returning, and between ticks no marks are set,
 * since the fighters' collision step clears its own (ADR-0006,
 * gameplay-state.md, "Is there ground below?"). */
static bool match_cast_floor(
    float x, float top, float bottom, int skip, int* line_out, float* y_out) {
    if (top <= bottom) {
        return false;
    }
    Vec3 hit;
    int line = -1;
    u32 flags;
    Vec3 normal;
    if (!mpCheckFloor(
            x, top, x, bottom, 0.0F, &hit, &line, &flags, &normal, skip, -1, -1, NULL, NULL) ||
        line < 0 || line >= mpLib_8004D164()->line_count)
    {
        return false;
    }
    *line_out = line;
    *y_out = hit.y;
    return true;
}

/* An end of the floor line's surface (A11yFloorEnd), the right one or the
 * left, and whether a fall from it is deadly: edge_offset past it, the
 * cast from just above its height down to the bottom blast zone. */
static void match_floor_end(
    int line, bool right, float edge_offset, float bottom, A11yFloorEnd* out) {
    int line_count = mpLib_8004D164()->line_count;
    Vec3 end;
    if (right) {
        mpFloorGetRight(line, &end);
    } else {
        mpFloorGetLeft(line, &end);
    }
    out->x = end.x;
    out->y = end.y;
    out->deadly = false;
    /* The line joined at that end, past the floor lines: on a ledge the
     * stage's side going down, at a step a wall going up, which no walk
     * leaves the floor by. The walk is the one mpFloorGetRight and
     * mpFloorGetLeft have just made unbounded, so it ends; the bound only
     * keeps the loop's end in sight. */
    int joined = -1;
    for (int steps = 0; steps < line_count; steps++) {
        int next = right ? mpLineGetNext(line) : mpLineGetPrev(line);
        if (next < 0 || next >= line_count || mpLineGetKind(next) != CollLine_Floor) {
            joined = next >= 0 && next < line_count ? next : -1;
            break;
        }
        line = next;
    }
    /* Only a valid id reaches mpLib_80054ED8: on any other it spins
     * forever. */
    if (joined >= 0 && mpLib_80054ED8(joined)) {
        Vec3 beyond;
        if (right) {
            mpLineGetV1Pos(joined, &beyond);
        } else {
            mpLineGetV0Pos(joined, &beyond);
        }
        if (beyond.y > end.y) {
            return;
        }
    }
    float x = right ? end.x + edge_offset : end.x - edge_offset;
    int below;
    float y;
    out->deadly = !match_cast_floor(x, end.y + A11Y_CAST_LIFT, bottom, -1, &below, &y);
}

/* The floor below a fighter in play (A11yMatchState.below), and its ends.
 * The bottom blast zone is where a fall would end. */
static void match_floor_below(
    const Fighter* fp, float edge_offset, float bottom, A11yFloorBelow* out) {
    out->found = false;
    out->line = -1;
    int line_count = mpLib_8004D164()->line_count;
    if (fp->ground_or_air == GA_Ground) {
        int line = fp->coll_data.floor.index;
        if (line >= 0 && line < line_count) {
            out->found = true;
            out->line = line;
            out->y = fp->cur_pos.y;
        }
    } else {
        /* From just above the bottom of the fighter's collision box, the
         * point the game lands it on: in the air the box rises above the
         * feet, and the feet sink up to about 6 units below a floor the box
         * is still above (measured on Link landing), so a cast from the
         * feet would find a gap over the floor. A platform the fighter is
         * dropping through is skipped, as the game's own collision step
         * skips it. */
        float top = fp->cur_pos.y + fp->coll_data.ecb.bottom.y + A11Y_CAST_LIFT;
        out->found = match_cast_floor(
            fp->cur_pos.x, top, bottom, fp->coll_data.floor_skip, &out->line, &out->y);
    }
    if (out->found) {
        match_floor_end(out->line, false, edge_offset, bottom, &out->left);
        match_floor_end(out->line, true, edge_offset, bottom, &out->right);
    }
}

/* The slot the local player plays: see A11yMatchState.you. */
static int match_your_slot(int local_port, bool online) {
    int first_human = -1;
    for (int slot = 0; slot < Gm_Player_NumMax; slot++) {
        if (Player_GetPlayerSlotType(slot) != Gm_PKind_Human) {
            continue;
        }
        if (Player_GetPadPort(slot) == local_port) {
            return slot;
        }
        if (first_human < 0) {
            first_human = slot;
        }
    }
    return online ? -1 : first_human;
}

void a11y_game_match_state(
    int local_port, bool online, uint64_t held_mask, float edge_offset, A11yMatchState* out) {
    memset(out, 0, sizeof(*out));
    out->below.line = -1;
    int your_slot = match_your_slot(local_port, online);
    const Fighter* you = your_slot >= 0 ? match_fighter(your_slot) : NULL;
    if (you == NULL) {
        return;
    }
    /* The box around the enabled floors, computed when a stage's geometry
     * is set up or moves, and left inside out until then. */
    const mpCollisionBox* floors = &mpLib_80458868[1];
    if (floors->left > floors->right || floors->bottom > floors->top) {
        return;
    }
    out->running = true;
    /* The pause word (gm_80479D58.unk_10.x0): bit 1 a VS pause, 2 a
     * Training pause, 3 the pause camera, 4 the GAME! freeze, 5 a
     * single-player score tally, 6 an event's freeze, 0 the debug pause. A
     * slow-motion mode skips ticks through the frame timer instead and
     * leaves it clear. A Training pause holds no GObj list: the fighters'
     * own update stands still on bit 2. */
    out->paused = gm_801A4624() != 0;
    out->fighters_held = (held_mask & (1ULL << HSD_GOBJ_PLINK_FIGHTER)) != 0;
    match_fighter_state(your_slot, you, &out->you);

    bool teams = gm_8016B168();
    float nearest = 0.0F; /* squared */
    for (int slot = 0; slot < Gm_Player_NumMax; slot++) {
        if (slot == your_slot) {
            continue;
        }
        const Fighter* fp = match_fighter(slot);
        if (fp == NULL) {
            continue;
        }
        A11yMatchFighter other;
        match_fighter_state(slot, fp, &other);
        if (other.out || (teams && other.team == out->you.team)) {
            continue;
        }
        float dx = other.x - out->you.x;
        float dy = other.y - out->you.y;
        float squared = dx * dx + dy * dy;
        if (out->opponent_count == 0 || squared < nearest) {
            nearest = squared;
            out->opponents[0] = other;
            out->opponent_count = 1;
        }
    }

    A11yStageFrame* stage = &out->stage;
    stage->blast_left = Stage_GetBlastZoneLeftOffset();
    stage->blast_right = Stage_GetBlastZoneRightOffset();
    stage->blast_top = Stage_GetBlastZoneTopOffset();
    stage->blast_bottom = Stage_GetBlastZoneBottomOffset();
    stage->camera_left = Stage_GetCamBoundsLeftOffset();
    stage->camera_right = Stage_GetCamBoundsRightOffset();
    stage->camera_top = Stage_GetCamBoundsTopOffset();
    stage->camera_bottom = Stage_GetCamBoundsBottomOffset();
    /* The floors' box alone would do on most tournament stages, but on
     * Yoshi's Story the cloud circling the stage drags its bottom down to
     * -33 and its sides out past the main floor's. */
    if (!match_main_floor((stage->blast_left + stage->blast_right) / 2.0F, stage)) {
        stage->floor_left = floors->left;
        stage->floor_right = floors->right;
        stage->floor_y = floors->bottom;
    }
    if (!out->you.out) {
        match_floor_below(you, edge_offset, stage->blast_bottom, &out->below);
    }
}
