/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

namespace a11y {

/* With MELEE_A11Y_TEXT_DUMP=1, logs every string of the main menu's loaded
 * string table (SdMenu) decoded, once per run, for checking the decoder
 * against a disc (docs/a11y/native-menus.md, "Decoding game text"). Called
 * every frame of the menu scene; does nothing until the table is loaded. */
void menu_text_dump_once();

}  // namespace a11y
