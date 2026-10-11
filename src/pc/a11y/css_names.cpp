/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_names.hpp"
#include "game_access.h"
#include "menu_kinds.h"
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

std::optional<std::string_view> mode_name(int match_type) {
    struct ModeWords {
        A11yCssMatchType match_type;
        std::string_view name;
    };
    constexpr ModeWords kModes[] = {
        {A11Y_REG_CLASSIC, "Classic"},
        {A11Y_REG_ADVENTURE, "Adventure"},
        {A11Y_REG_ALLSTAR, "All-Star"},
        {A11Y_EVENT_MATCH, "Event Match"},
        {A11Y_STADIUM_TARGET, "Target Test"},
        {A11Y_STADIUM_HOMERUN, "Home-Run Contest"},
        {A11Y_STADIUM_MULTIMAN_10, "10-Man Melee"},
        {A11Y_STADIUM_MULTIMAN_100, "100-Man Melee"},
        {A11Y_STADIUM_3_MIN_MELEE, "3-Minute Melee"},
        {A11Y_STADIUM_15_MIN_MELEE, "15-Minute Melee"},
        {A11Y_STADIUM_ENDLESS_MELEE, "Endless Melee"},
        {A11Y_STADIUM_CRUEL_MELEE, "Cruel Melee"},
        {A11Y_TRAINING_MODE, "Training"},
    };
    for (const ModeWords& mode : kModes) {
        if (mode.match_type == match_type) {
            return mode.name;
        }
    }
    return std::nullopt;
}

std::string rules_header(int match_type, const A11yRules& rules) {
    enum class RulesMode : int {
#define A11Y_RULES_MODE_ENUMERATOR(name, number) name = number,
        A11Y_RULES_MODES(A11Y_RULES_MODE_ENUMERATOR)
#undef A11Y_RULES_MODE_ENUMERATOR
    };
    if (match_type == A11Y_CSS_VS_CAMERA) {
        return "Take Snapshots!";
    }
    /* The pieces after the number: 0x4B to 0x4E by mode, and for stock
     * 0x54 or 0x55 before 0x4C. A mode the game does not use is said as
     * time's, as PAL draws it. */
    std::string_view tail;
    switch (static_cast<RulesMode>(rules.mode)) {
    case RulesMode::Mode_Stock:
        return std::to_string(rules.stock_count) + (rules.stock_count < 2 ? "-life" : "-man") +
               " survival test!";
    case RulesMode::Mode_Coin:
        tail = "coin fest!";
        break;
    case RulesMode::Mode_Bonus:
        tail = "skills test!";
        break;
    case RulesMode::Mode_Time:
    default:
        tail = "KO fest!";
        break;
    }
    /* "An endless" (0x51) without a time limit, "-minute" (0x53) with one. */
    std::string limit =
        rules.time_limit == 0 ? "An endless" : std::to_string(rules.time_limit) + "-minute";
    return limit + " " + std::string(tail);
}

std::optional<std::string_view> difficulty_word(int difficulty) {
    /* The strip's texts in mnCharSel_802640A0, by the value they stand for. */
    constexpr std::string_view kDifficulties[] = {
        "Very easy", "Easy", "Normal", "Hard", "Very hard"};
    if (difficulty < 0 || difficulty >= static_cast<int>(std::size(kDifficulties))) {
        return std::nullopt;
    }
    return kDifficulties[difficulty];
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
