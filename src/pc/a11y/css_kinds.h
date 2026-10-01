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

/* X(name, number) for the single-player modes' CSSMatchType values
 * (src/melee/mn/types.h), the screen's mode, mirrored as character_kinds.h
 * mirrors the characters: game_access.c checks every number against the
 * decomp's own. The VS modes come before REG_CLASSIC. */
#define A11Y_CSS_MATCH_TYPES(X)                                                                    \
    X(REG_CLASSIC, 0xB)                                                                            \
    X(REG_ADVENTURE, 0xC)                                                                          \
    X(REG_ALLSTAR, 0xD)                                                                            \
    X(EVENT_MATCH, 0xE)                                                                            \
    X(STADIUM_TARGET, 0xF)                                                                         \
    X(STADIUM_HOMERUN, 0x10)                                                                       \
    X(STADIUM_MULTIMAN_10, 0x11)                                                                   \
    X(STADIUM_MULTIMAN_100, 0x12)                                                                  \
    X(STADIUM_3_MIN_MELEE, 0x13)                                                                   \
    X(STADIUM_15_MIN_MELEE, 0x14)                                                                  \
    X(STADIUM_ENDLESS_MELEE, 0x15)                                                                 \
    X(STADIUM_CRUEL_MELEE, 0x16)                                                                   \
    X(TRAINING_MODE, 0x17)

typedef enum A11yCssMatchType {
#define A11Y_CSS_MATCH_TYPE_ENUMERATOR(name, number) A11Y_##name = (number),
    A11Y_CSS_MATCH_TYPES(A11Y_CSS_MATCH_TYPE_ENUMERATOR)
#undef A11Y_CSS_MATCH_TYPE_ENUMERATOR
} A11yCssMatchType;
