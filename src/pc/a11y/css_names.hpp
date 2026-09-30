/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select's string table: the characters' names as drawn under their
 * portraits on the English screen, in normal capitalisation ("C.FALCON" is
 * "C. Falcon"), so the blind player hears what a sighted player reads
 * (.scratch/character-select/spec.md, "Where the words come from"). The game
 * draws them as pictures, so there is no game text to read. */
#pragma once
#include "character_kinds.h"
#include <optional>
#include <string_view>

namespace a11y {

/* A character's name; nothing for none or a kind missing from the table. */
std::optional<std::string_view> character_name(A11yCharacter character);

}  // namespace a11y
