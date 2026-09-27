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

#ifdef __cplusplus
}
#endif
