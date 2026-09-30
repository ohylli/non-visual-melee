/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's kinds that fork code names without reading game memory,
 * apart from game_access.h so the string table can name them too. */
#pragma once

/* What a player slot's HMN/CPU tab shows. */
typedef enum A11yCssSlotKind {
    A11Y_CSS_HUMAN,
    A11Y_CSS_CPU,
    /* Drawn "N/A". A closed slot shows no coin, but its character may hold a
     * stale value. */
    A11Y_CSS_CLOSED,
} A11yCssSlotKind;
