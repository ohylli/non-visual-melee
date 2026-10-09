/* SPDX-License-Identifier: GPL-3.0-or-later */
/* StKind (src/melee/gr/forward.h) mirrored for fork code, which does not
 * include the decomp's headers. game_access.c checks every number against the
 * decomp's own at compile time, so a base merge that renumbers the stages
 * fails the build instead of naming the wrong one. Only the 29 stages of
 * stage select's grid are here; the single-player modes' stages never reach
 * it. */
#pragma once

/* X(name, number) for every stage on stage select, in the decomp's order. */
#define A11Y_STAGE_KINDS(X)                                                                        \
    X(St_Kind_Izumi, 0x02)                                                                         \
    X(St_Kind_PStadium, 0x03)                                                                      \
    X(St_Kind_Castle, 0x04)                                                                        \
    X(St_Kind_Kongo, 0x05)                                                                         \
    X(St_Kind_Zebes, 0x06)                                                                         \
    X(St_Kind_Corneria, 0x07)                                                                      \
    X(St_Kind_Story, 0x08)                                                                         \
    X(St_Kind_Onett, 0x09)                                                                         \
    X(St_Kind_MuteCity, 0x0A)                                                                      \
    X(St_Kind_RCruise, 0x0B)                                                                       \
    X(St_Kind_Garden, 0x0C)                                                                        \
    X(St_Kind_GreatBay, 0x0D)                                                                      \
    X(St_Kind_Shrine, 0x0E)                                                                        \
    X(St_Kind_Kraid, 0x0F)                                                                         \
    X(St_Kind_Yoster, 0x10)                                                                        \
    X(St_Kind_Greens, 0x11)                                                                        \
    X(St_Kind_Fourside, 0x12)                                                                      \
    X(St_Kind_Inishie1, 0x13)                                                                      \
    X(St_Kind_Inishie2, 0x14)                                                                      \
    X(St_Kind_Venom, 0x16)                                                                         \
    X(St_Kind_Pura, 0x17)                                                                          \
    X(St_Kind_BigBlue, 0x18)                                                                       \
    X(St_Kind_Icemt, 0x19)                                                                         \
    X(St_Kind_Flatzone, 0x1B)                                                                      \
    X(St_Kind_OldPupupu, 0x1C)                                                                     \
    X(St_Kind_OldYoshi, 0x1D)                                                                      \
    X(St_Kind_OldKongo, 0x1E)                                                                      \
    X(St_Kind_Battle, 0x1F)                                                                        \
    X(St_Kind_Last, 0x20)

/* A stage as fork code holds it, in C and in C++: the kinds under the fork's
 * own names (A11Y_St_Kind_Corneria), and none, which is also what the random
 * tile holds. A C enum for the same reason as A11yCharacter. */
typedef enum A11yStage {
    A11Y_NO_STAGE = -1,
#define A11Y_STAGE_ENUMERATOR(name, number) A11Y_##name = (number),
    A11Y_STAGE_KINDS(A11Y_STAGE_ENUMERATOR)
#undef A11Y_STAGE_ENUMERATOR
} A11yStage;
