/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>

namespace a11y {

/* The main menu tree put up the description line of an entry. For now this
 * only logs the line decoded, the experiment that tests decoding game text
 * (docs/a11y/native-menus.md, Open questions). MELEE_A11Y_TEXT_DUMP=1 also
 * logs every string of the loaded table once. */
void menu_description_shown(int menu_kind, int selection, const std::uint8_t* bytes);

}  // namespace a11y
