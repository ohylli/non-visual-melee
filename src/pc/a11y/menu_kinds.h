/* SPDX-License-Identifier: GPL-3.0-or-later */
/* MenuKind and the tree screens' selection enums (src/melee/mn/forward.h)
 * mirrored for fork code, which does not include the decomp's headers.
 * game_access.c checks every number against the decomp's own at compile time,
 * so a base merge that renumbers a screen or an entry fails the build instead
 * of naming the wrong one. */
#pragma once

/* X(name, number) for every named kind; MENU_KIND_34, the retail table length,
 * is left out: on PC its number is MENU_KIND_ONLINE's. */
#define A11Y_MENU_KINDS(X)                                                                         \
    X(MENU_KIND_MAIN, 0)                                                                           \
    X(MENU_KIND_1P, 1)                                                                             \
    X(MENU_KIND_VS, 2)                                                                             \
    X(MENU_KIND_TOY, 3)                                                                            \
    X(MENU_KIND_SETTINGS, 4)                                                                       \
    X(MENU_KIND_DATA, 5)                                                                           \
    X(MENU_KIND_REG, 6)                                                                            \
    X(MENU_KIND_EVENT, 7)                                                                          \
    X(MENU_KIND_8, 8)                                                                              \
    X(MENU_KIND_STADIUM, 9)                                                                        \
    X(MENU_KIND_10, 10)                                                                            \
    X(MENU_KIND_11, 11)                                                                            \
    X(MENU_KIND_SPECIAL, 12)                                                                       \
    X(MENU_KIND_RULES, 13)                                                                         \
    X(MENU_KIND_14, 14)                                                                            \
    X(MENU_KIND_RULES_EXTRA, 15)                                                                   \
    X(MENU_KIND_RULES_ITEMS, 16)                                                                   \
    X(MENU_KIND_RULES_STAGE, 17)                                                                   \
    X(MENU_KIND_NAME_ENTRY, 18)                                                                    \
    X(MENU_KIND_SETTINGS_RUMBLE, 19)                                                               \
    X(MENU_KIND_SETTINGS_SOUND, 20)                                                                \
    X(MENU_KIND_DISPLAY, 21)                                                                       \
    X(MENU_KIND_22, 22)                                                                            \
    X(MENU_KIND_SETTINGS_LANG, 23)                                                                 \
    X(MENU_KIND_SETTINGS_ERASE, 24)                                                                \
    X(MENU_KIND_DATA_SNAP, 25)                                                                     \
    X(MENU_KIND_DATA_ARCHIVES, 26)                                                                 \
    X(MENU_KIND_27, 27)                                                                            \
    X(MENU_KIND_RECORDS, 28)                                                                       \
    X(MENU_KIND_DATA_SPECIAL, 29)                                                                  \
    X(MENU_KIND_RECORDS_VS, 30)                                                                    \
    X(MENU_KIND_RECORDS_BONUS, 31)                                                                 \
    X(MENU_KIND_RECORDS_MISC, 32)                                                                  \
    X(MENU_KIND_MULTI_VS, 33)                                                                      \
    X(MENU_KIND_ONLINE, 34)

/* X(name, number) for the entries of the tree screens the fork names from
 * its own table. The removed entries (SEL_1P_2, SEL_TOY_2, SEL_SETTINGS_3)
 * and the Online screen's, which the base port names at runtime, are left
 * out. */
#define A11Y_MENU_SELECTIONS(X)                                                                    \
    X(SEL_MAIN_1P, 0)                                                                              \
    X(SEL_MAIN_VS, 1)                                                                              \
    X(SEL_MAIN_TOY, 2)                                                                             \
    X(SEL_MAIN_SETTINGS, 3)                                                                        \
    X(SEL_MAIN_DATA, 4)                                                                            \
    X(SEL_1P_REG, 0)                                                                               \
    X(SEL_1P_EVENT, 1)                                                                             \
    X(SEL_1P_STADIUM, 3)                                                                           \
    X(SEL_1P_TRAINING, 4)                                                                          \
    X(SEL_REG_CLASSIC, 0)                                                                          \
    X(SEL_REG_ADVENTURE, 1)                                                                        \
    X(SEL_REG_ALLSTAR, 2)                                                                          \
    X(SEL_STADIUM_TARGET, 0)                                                                       \
    X(SEL_STADIUM_HOMERUN, 1)                                                                      \
    X(SEL_STADIUM_MULTIMAN, 2)                                                                     \
    X(SEL_VS_MELEE, 0)                                                                             \
    X(SEL_VS_TOURNAMENT, 1)                                                                        \
    X(SEL_VS_SPECIAL, 2)                                                                           \
    X(SEL_VS_RULES, 3)                                                                             \
    X(SEL_VS_NAME, 4)                                                                              \
    X(SEL_VS_ONLINE, 5)                                                                            \
    X(SEL_SPECIAL_VS_CAMERA, 0)                                                                    \
    X(SEL_SPECIAL_VS_STAMINA, 1)                                                                   \
    X(SEL_SPECIAL_VS_SUDDEN_DEATH, 2)                                                              \
    X(SEL_SPECIAL_VS_GIANT, 3)                                                                     \
    X(SEL_SPECIAL_VS_TINY, 4)                                                                      \
    X(SEL_SPECIAL_VS_INVISIBLE, 5)                                                                 \
    X(SEL_SPECIAL_VS_FIXED_CAMERA, 6)                                                              \
    X(SEL_SPECIAL_VS_SINGLE_BUTTON, 7)                                                             \
    X(SEL_SPECIAL_VS_LIGHTNING, 8)                                                                 \
    X(SEL_SPECIAL_VS_SLOMO, 9)                                                                     \
    X(SEL_TOY_GALLERY, 0)                                                                          \
    X(SEL_TOY_LOTTERY, 1)                                                                          \
    X(SEL_TOY_COLLECTION, 3)                                                                       \
    X(SEL_SETTINGS_RUMBLE, 0)                                                                      \
    X(SEL_SETTINGS_SOUND, 1)                                                                       \
    X(SEL_SETTINGS_DISPLAY, 2)                                                                     \
    X(SEL_SETTINGS_LANG, 4)                                                                        \
    X(SEL_SETTINGS_ERASE, 5)                                                                       \
    X(SEL_DATA_SNAP, 0)                                                                            \
    X(SEL_DATA_ARCHIVES, 1)                                                                        \
    X(SEL_DATA_SOUND, 2)                                                                           \
    X(SEL_DATA_RECORDS, 3)                                                                         \
    X(SEL_DATA_SPECIAL, 4)                                                                         \
    X(SEL_RECORDS_VS, 0)                                                                           \
    X(SEL_RECORDS_BONUS, 1)                                                                        \
    X(SEL_RECORDS_MISC, 2)
