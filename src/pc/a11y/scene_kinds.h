/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GameSceneKind (src/melee/gm/forward.h) mirrored for fork code, which does
 * not include the decomp's headers. game_access.c checks every number against
 * the decomp's own at compile time, so a base merge that renumbers the scenes
 * fails the build instead of naming the wrong one. A kind the base port adds
 * at the end is simply missing here until it is added. */
#pragma once

/* X(name, number) for every kind, in the decomp's order. */
#define A11Y_SCENE_KINDS(X)                                                                        \
    X(GS_TITLE, 0x00)                                                                              \
    X(GS_MENU, 0x01)                                                                               \
    X(GS_VS, 0x02)                                                                                 \
    X(GS_SUDDEN_DEATH, 0x03)                                                                       \
    X(GS_TRAINING, 0x04)                                                                           \
    X(GS_RESULTS, 0x05)                                                                            \
    X(GS_0x6, 0x06)                                                                                \
    X(GS_DEBUG_MENU, 0x07)                                                                         \
    X(GS_CSS, 0x08)                                                                                \
    X(GS_SSS, 0x09)                                                                                \
    X(GS_UNK10, 0x0A)                                                                              \
    X(GS_TOY_GALLERY, 0x0B)                                                                        \
    X(GS_TOY_LOTTERY, 0x0C)                                                                        \
    X(GS_TOY_COLLECTION, 0x0D)                                                                     \
    X(GS_INTRO_NORMAL, 0x0E)                                                                       \
    X(GS_REGEND_TOYFALL, 0x0F)                                                                     \
    X(GS_REGEND_CONGRATS, 0x10)                                                                    \
    X(GS_CUTSCENE_LUIGI, 0x11)                                                                     \
    X(GS_CUTSCENE_BRINSTAR, 0x12)                                                                  \
    X(GS_CUTSCENE_EXPLOSION, 0x13)                                                                 \
    X(GS_CUTSCENE_3KIRBYS, 0x14)                                                                   \
    X(GS_CUTSCENE_GIANTKIRBY, 0x15)                                                                \
    X(GS_CUTSCENE_STARFOX, 0x16)                                                                   \
    X(GS_CUTSCENE_FZERO, 0x17)                                                                     \
    X(GS_CUTSCENE_METAL, 0x18)                                                                     \
    X(GS_CUTSCENE_BOWSERTOY, 0x19)                                                                 \
    X(GS_CUTSCENE_GIGATRANSFORM, 0x1A)                                                             \
    X(GS_CUTSCENE_GIGADEFEATED, 0x1B)                                                              \
    X(GS_MOVIE_OPENING, 0x1C)                                                                      \
    X(GS_MOVIE_END, 0x1D)                                                                          \
    X(GS_MOVIE_HOWTO, 0x1E)                                                                        \
    X(GS_MOVIE_OMAKE15, 0x1F)                                                                      \
    X(GS_INTRO_EASY, 0x20)                                                                         \
    X(GS_INTRO_ALLSTAR, 0x21)                                                                      \
    X(GS_GAMEOVER, 0x22)                                                                           \
    X(GS_COMING_SOON, 0x23)                                                                        \
    X(GS_TOU_SETUP, 0x24)                                                                          \
    X(GS_TOU_BRACKET, 0x25)                                                                        \
    X(GS_TOU_ALT, 0x26)                                                                            \
    X(GS_PRIZE_INTERFACE, 0x27)                                                                    \
    X(GS_PROG_SCAN, 0x28)                                                                          \
    X(GS_APPROACH, 0x29)                                                                           \
    X(GS_MEMCARD, 0x2A)                                                                            \
    X(GS_STAFFROLL, 0x2B)                                                                          \
    X(GS_CAMERA_VS, 0x2C)                                                                          \
    X(GS_ONLINE_LOBBY, 0x2D)
