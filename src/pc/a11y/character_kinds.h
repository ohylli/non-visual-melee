/* SPDX-License-Identifier: GPL-3.0-or-later */
/* CharacterKind (src/melee/ft/forward.h) mirrored for fork code, which does
 * not include the decomp's headers. game_access.c checks every number against
 * the decomp's own at compile time, so a base merge that renumbers the
 * characters fails the build instead of naming the wrong one. Only the
 * playable characters are here; the bosses and wireframes never reach
 * character select. */
#pragma once

/* X(name, number) for every playable kind, in the decomp's order. */
#define A11Y_CHARACTER_KINDS(X)                                                                    \
    X(CKind_Captain, 0x00)                                                                         \
    X(CKind_Donkey, 0x01)                                                                          \
    X(CKind_Fox, 0x02)                                                                             \
    X(CKind_GameWatch, 0x03)                                                                       \
    X(CKind_Kirby, 0x04)                                                                           \
    X(CKind_Koopa, 0x05)                                                                           \
    X(CKind_Link, 0x06)                                                                            \
    X(CKind_Luigi, 0x07)                                                                           \
    X(CKind_Mario, 0x08)                                                                           \
    X(CKind_Mars, 0x09)                                                                            \
    X(CKind_Mewtwo, 0x0A)                                                                          \
    X(CKind_Ness, 0x0B)                                                                            \
    X(CKind_Peach, 0x0C)                                                                           \
    X(CKind_Pikachu, 0x0D)                                                                         \
    X(CKind_PopoNana, 0x0E)                                                                        \
    X(CKind_Purin, 0x0F)                                                                           \
    X(CKind_Samus, 0x10)                                                                           \
    X(CKind_Yoshi, 0x11)                                                                           \
    X(CKind_Zelda, 0x12)                                                                           \
    X(CKind_Seak, 0x13)                                                                            \
    X(CKind_Falco, 0x14)                                                                           \
    X(CKind_CLink, 0x15)                                                                           \
    X(CKind_DrMario, 0x16)                                                                         \
    X(CKind_Emblem, 0x17)                                                                          \
    X(CKind_Pichu, 0x18)                                                                           \
    X(CKind_Ganon, 0x19)
