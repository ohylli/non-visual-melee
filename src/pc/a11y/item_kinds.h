/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ItemKind (src/melee/it/forward.h) mirrored for fork code, which does not
 * include the decomp's headers. game_access.c checks every number against the
 * decomp's own at compile time, so a base merge that renumbers the items
 * fails the build instead of naming the wrong one. Only the items a menu names
 * are here: Item Switch's 31, of which the Training menu offers all but the
 * Party Ball. */
#pragma once

/* X(name, number) for every item a menu names, in the decomp's order. */
#define A11Y_ITEM_KINDS(X)                                                                         \
    X(It_Kind_Kusudama, 0x04)                                                                      \
    X(It_Kind_TaruCann, 0x05)                                                                      \
    X(It_Kind_BombHei, 0x06)                                                                       \
    X(It_Kind_Dosei, 0x07)                                                                         \
    X(It_Kind_Heart, 0x08)                                                                         \
    X(It_Kind_Tomato, 0x09)                                                                        \
    X(It_Kind_Star, 0x0A)                                                                          \
    X(It_Kind_Bat, 0x0B)                                                                           \
    X(It_Kind_Sword, 0x0C)                                                                         \
    X(It_Kind_Parasol, 0x0D)                                                                       \
    X(It_Kind_G_Shell, 0x0E)                                                                       \
    X(It_Kind_R_Shell, 0x0F)                                                                       \
    X(It_Kind_L_Gun, 0x10)                                                                         \
    X(It_Kind_Freeze, 0x11)                                                                        \
    X(It_Kind_Foods, 0x12)                                                                         \
    X(It_Kind_MSBomb, 0x13)                                                                        \
    X(It_Kind_Flipper, 0x14)                                                                       \
    X(It_Kind_S_Scope, 0x15)                                                                       \
    X(It_Kind_StarRod, 0x16)                                                                       \
    X(It_Kind_LipStick, 0x17)                                                                      \
    X(It_Kind_Harisen, 0x18)                                                                       \
    X(It_Kind_F_Flower, 0x19)                                                                      \
    X(It_Kind_Kinoko, 0x1A)                                                                        \
    X(It_Kind_DKinoko, 0x1B)                                                                       \
    X(It_Kind_Hammer, 0x1C)                                                                        \
    X(It_Kind_WStar, 0x1D)                                                                         \
    X(It_Kind_ScBall, 0x1E)                                                                        \
    X(It_Kind_RabbitC, 0x1F)                                                                       \
    X(It_Kind_MetalB, 0x20)                                                                        \
    X(It_Kind_Spycloak, 0x21)                                                                      \
    X(It_Kind_M_Ball, 0x22)

/* An item as fork code holds it, in C and in C++: the kinds under the fork's
 * own names (A11Y_It_Kind_Foods), and none. A C enum for the same reason as
 * A11yCharacter. */
typedef enum A11yItem {
    A11Y_NO_ITEM = -1,
#define A11Y_ITEM_ENUMERATOR(name, number) A11Y_##name = (number),
    A11Y_ITEM_KINDS(A11Y_ITEM_ENUMERATOR)
#undef A11Y_ITEM_ENUMERATOR
} A11yItem;
