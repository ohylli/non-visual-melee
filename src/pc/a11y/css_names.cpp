/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_names.hpp"
#include <iterator>

namespace a11y {
namespace {

struct CharacterWords {
    A11yCharacter character;
    std::string_view name;
};

/* Every playable kind, each once, in the order of the portraits: the top row,
 * the middle row, the bottom row, each from the left. Sheik has no portrait
 * of her own; she is here for a slot that keeps her after a match. */
constexpr CharacterWords kCharacters[] = {
    {A11Y_CKind_DrMario, "Dr. Mario"},
    {A11Y_CKind_Mario, "Mario"},
    {A11Y_CKind_Luigi, "Luigi"},
    {A11Y_CKind_Koopa, "Bowser"},
    {A11Y_CKind_Peach, "Peach"},
    {A11Y_CKind_Yoshi, "Yoshi"},
    {A11Y_CKind_Donkey, "DK"},
    {A11Y_CKind_Captain, "C. Falcon"},
    {A11Y_CKind_Ganon, "Ganondorf"},

    {A11Y_CKind_Falco, "Falco"},
    {A11Y_CKind_Fox, "Fox"},
    {A11Y_CKind_Ness, "Ness"},
    {A11Y_CKind_PopoNana, "Ice Climbers"},
    {A11Y_CKind_Kirby, "Kirby"},
    {A11Y_CKind_Samus, "Samus"},
    {A11Y_CKind_Zelda, "Zelda"},
    {A11Y_CKind_Link, "Link"},
    {A11Y_CKind_CLink, "Young Link"},

    {A11Y_CKind_Pichu, "Pichu"},
    {A11Y_CKind_Pikachu, "Pikachu"},
    {A11Y_CKind_Purin, "Jigglypuff"},
    {A11Y_CKind_Mewtwo, "Mewtwo"},
    {A11Y_CKind_GameWatch, "Mr. Game & Watch"},
    {A11Y_CKind_Mars, "Marth"},
    {A11Y_CKind_Emblem, "Roy"},

    {A11Y_CKind_Seak, "Sheik"},
};

constexpr A11yCharacter kAllCharacters[] = {
#define A11Y_CHARACTER_ITEM(name, number) A11Y_##name,
    A11Y_CHARACTER_KINDS(A11Y_CHARACTER_ITEM)
#undef A11Y_CHARACTER_ITEM
};

constexpr bool table_has_every_character_once() {
    for (A11yCharacter character : kAllCharacters) {
        int found = 0;
        for (const CharacterWords& words : kCharacters) {
            found += words.character == character ? 1 : 0;
        }
        if (found != 1) {
            return false;
        }
    }
    return std::size(kCharacters) == std::size(kAllCharacters);
}
static_assert(table_has_every_character_once(), "kCharacters needs every character, each once");

}  // namespace

std::optional<std::string_view> character_name(A11yCharacter character) {
    for (const CharacterWords& words : kCharacters) {
        if (words.character == character) {
            return words.name;
        }
    }
    return std::nullopt;
}

std::string_view slot_kind_word(A11yCssSlotKind kind) {
    switch (kind) {
    case A11Y_CSS_HUMAN:
        return "human";
    case A11Y_CSS_CPU:
        return "CPU";
    case A11Y_CSS_CLOSED:
        break;
    }
    return "closed";
}

std::optional<std::string_view> team_word(int team) {
    /* The door's team, 0 to 2, shows the colours of ports 1, 2 and 4
     * (mnCharSel_804D50E0). */
    constexpr std::string_view kTeams[] = {"red", "blue", "green"};
    if (team < 0 || team >= static_cast<int>(std::size(kTeams))) {
        return std::nullopt;
    }
    return kTeams[team];
}

}  // namespace a11y
