/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

namespace a11y {

/* With MELEE_A11Y_TEXT_DUMP=1, logs every string of the string table loaded
 * in font slot 0 decoded, once per table and run, for checking the decoder
 * against a disc (docs/a11y/native-menus.md, "Decoding game text"): SdMenu
 * in the menu scene, SdSlChr on character select. Called every frame of
 * those scenes; does nothing until a table is loaded. */
void menu_text_dump_once();

}  // namespace a11y
