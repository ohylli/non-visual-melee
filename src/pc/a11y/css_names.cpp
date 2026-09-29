/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_names.hpp"
#include <iterator>

namespace a11y {
namespace {

struct CharacterWords {
    CharacterKind kind;
    std::string_view name;
};

/* Every playable kind, each once, in the order of the portraits: the top row,
 * the middle row, the bottom row, each from the left. Sheik has no portrait
 * of her own; she is here for a slot that keeps her after a match. */
constexpr CharacterWords kCharacters[] = {
    {CharacterKind::CKind_DrMario, "Dr. Mario"},
    {CharacterKind::CKind_Mario, "Mario"},
    {CharacterKind::CKind_Luigi, "Luigi"},
    {CharacterKind::CKind_Koopa, "Bowser"},
    {CharacterKind::CKind_Peach, "Peach"},
    {CharacterKind::CKind_Yoshi, "Yoshi"},
    {CharacterKind::CKind_Donkey, "DK"},
    {CharacterKind::CKind_Captain, "C. Falcon"},
    {CharacterKind::CKind_Ganon, "Ganondorf"},

    {CharacterKind::CKind_Falco, "Falco"},
    {CharacterKind::CKind_Fox, "Fox"},
    {CharacterKind::CKind_Ness, "Ness"},
    {CharacterKind::CKind_PopoNana, "Ice Climbers"},
    {CharacterKind::CKind_Kirby, "Kirby"},
    {CharacterKind::CKind_Samus, "Samus"},
    {CharacterKind::CKind_Zelda, "Zelda"},
    {CharacterKind::CKind_Link, "Link"},
    {CharacterKind::CKind_CLink, "Young Link"},

    {CharacterKind::CKind_Pichu, "Pichu"},
    {CharacterKind::CKind_Pikachu, "Pikachu"},
    {CharacterKind::CKind_Purin, "Jigglypuff"},
    {CharacterKind::CKind_Mewtwo, "Mewtwo"},
    {CharacterKind::CKind_GameWatch, "Mr. Game & Watch"},
    {CharacterKind::CKind_Mars, "Marth"},
    {CharacterKind::CKind_Emblem, "Roy"},

    {CharacterKind::CKind_Seak, "Sheik"},
};

constexpr CharacterKind kAllKinds[] = {
#define A11Y_CHARACTER_KIND_ITEM(name, number) CharacterKind::name,
    A11Y_CHARACTER_KINDS(A11Y_CHARACTER_KIND_ITEM)
#undef A11Y_CHARACTER_KIND_ITEM
};

constexpr bool table_has_every_kind_once() {
    for (CharacterKind kind : kAllKinds) {
        int found = 0;
        for (const CharacterWords& character : kCharacters) {
            found += character.kind == kind ? 1 : 0;
        }
        if (found != 1) {
            return false;
        }
    }
    return std::size(kCharacters) == std::size(kAllKinds);
}
static_assert(table_has_every_kind_once(), "kCharacters needs every character kind, each once");

}  // namespace

std::optional<std::string_view> character_name(int character) {
    for (const CharacterWords& words : kCharacters) {
        if (static_cast<int>(words.kind) == character) {
            return words.name;
        }
    }
    return std::nullopt;
}

}  // namespace a11y
