/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Every hook from the base port into the accessibility fork. A hook is one
 * call placed where something meaningful to the player happens; the fork
 * decides what to say. `grep -rn pc_a11y_ src --exclude-dir=a11y` lists every
 * call site. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Startup, before the launcher window opens (src/pc/main.c). Starts speech and
 * speaks the proof-of-life announcement. */
void pc_a11y_init(void);
/* First step of shutdown (src/pc/main.c), also reached when the window is
 * closed. */
void pc_a11y_shutdown(void);

/* Once per turn of the launcher's loop (src/pc/launcher.cpp), after the page
 * has been drawn. Launcher speech compares the page with the last turn here. */
void pc_a11y_launcher_frame(void);

/* A scene has loaded (gm_801A4014 in src/melee/gm/gm_1A3F.c, after the
 * scene's enter function returns) and its first frame is next. mode_kind is
 * the GameModeKind of the mode being run, which during a memory card
 * interruption differs from gm_GetCurrentGameMode(); scene_kind is the
 * GameSceneKind. */
void pc_a11y_scene_entered(int mode_kind, int scene_kind);

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

/* Character select's state lives in static variables of
 * src/melee/mn/mncharsel.c, so its hooks hand the fork what it reads. Only
 * game_access.c looks inside these types. */
struct CSSData;
struct CSSDoorsData;
struct CSSIcon;
struct CSSTag;
struct HSD_JObj;

/* Once a frame of character select (mnCharSel_Scene_OnFrame), before the
 * hands update: the screen's data, its four player slots, its table of 25
 * portraits, the players' name tag windows, the root of the screen's model
 * (where the sliders and name boxes are), the number of hands (4 in VS modes,
 * 1 in single-player modes), the pending exit (nonzero once the screen has
 * begun to leave or asked for the rules screen or name entry) and the Ready
 * to Fight banner (nonzero while shown). Character select speech compares
 * the screen with the last frame here. */
void pc_a11y_css_frame(const struct CSSData* css, const struct CSSDoorsData* doors,
    const struct CSSIcon* icons, const struct CSSTag* tags, struct HSD_JObj* models, int hand_count,
    int pending_exit, int ready);

/* One hand of character select has updated (the end of
 * mnCharSel_CursorThink). The raw numbers of the hand's struct, which only
 * mncharsel.c defines: its index (the port in VS modes), state, what it
 * holds, and position. The next frame reads them. */
void pc_a11y_css_hand(int hand, int state, int held, float x, float y);

#ifdef __cplusplus
}
#endif
