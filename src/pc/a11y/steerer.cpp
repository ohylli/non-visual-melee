/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "steerer.hpp"
#include "pc/pc.h"

namespace a11y {

bool pressed_direction(const A11yPad& pad, Direction* out) {
    if (pad.pressed_left) {
        *out = Direction::left;
    } else if (pad.pressed_right) {
        *out = Direction::right;
    } else if (pad.pressed_up) {
        *out = Direction::up;
    } else if (pad.pressed_down) {
        *out = Direction::down;
    } else {
        return false;
    }
    return true;
}

const char* why_ended(GlideEnd end) {
    switch (end) {
    case GlideEnd::failed:
        return "could not get there";
    case GlideEnd::abandoned:
        return "the player's stick took over";
    case GlideEnd::none:
    case GlideEnd::arrived:
        break;
    }
    return "arrived";
}

void log_glide_start(const std::string& words, Point aim) {
    pc_log_line("[a11y] glide to \"%s\" at (%.2f, %.2f)", words.c_str(), aim.x, aim.y);
}

void log_glide_end(const std::string& words, int frames, const char* why) {
    pc_log_line("[a11y] glide to \"%s\" ended after %d frames: %s", words.c_str(), frames, why);
}

}  // namespace a11y
