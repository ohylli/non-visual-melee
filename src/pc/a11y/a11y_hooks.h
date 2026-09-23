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

#ifdef __cplusplus
}
#endif
