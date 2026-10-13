/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The one translation unit that compiles miniaudio, in its own library
 * (a11y.cmake). Its device code stays out (MA_NO_DEVICE_IO): SDL plays the
 * cue mix. */
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
