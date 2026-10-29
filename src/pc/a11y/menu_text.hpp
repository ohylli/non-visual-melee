/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

namespace a11y {

/* With MELEE_A11Y_TEXT_DUMP=1, logs every string of the string table loaded
 * in a font slot decoded, once per table and run, for checking the decoder
 * against a disc (docs/a11y/native-menus.md, "Decoding game text"): in slot
 * 0 SdMenu in the menu scene, SdSlChr on character select, SdRst on
 * results; in slot 2 SdPrize at each unlock notice; in slot 3 SdMsgBox in
 * the memory card scene. Called every frame or tick of those scenes, or at
 * each notice; does nothing until a table is loaded. */
void menu_text_dump_once(int font = 0);

}  // namespace a11y
