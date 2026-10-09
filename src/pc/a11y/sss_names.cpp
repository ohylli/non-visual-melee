/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sss_names.hpp"
#include <iterator>

namespace a11y {
namespace {

struct StageWords {
    A11yStage stage;
    std::string_view name;
};

/* Every stage on the grid, each once, in the order of the tiles: Icicle
 * Mountain at the far left, the two rows of stacked pairs (the top row, then
 * the row under it), Flat Zone at the far right, the block beside the random
 * tile and the locked row under it, then the smaller bottom row. */
constexpr StageWords kStages[] = {
    {A11Y_St_Kind_Icemt, "Icicle Mountain"},
    {A11Y_St_Kind_Castle, "Princess Peach's Castle"},
    {A11Y_St_Kind_Kongo, "Kongo Jungle"},
    {A11Y_St_Kind_GreatBay, "Great Bay"},
    {A11Y_St_Kind_Story, "Yoshi's Story"},
    {A11Y_St_Kind_Izumi, "Fountain of Dreams"},
    {A11Y_St_Kind_Corneria, "Corneria"},
    {A11Y_St_Kind_Flatzone, "Flat Zone"},

    {A11Y_St_Kind_RCruise, "Rainbow Cruise"},
    {A11Y_St_Kind_Garden, "Jungle Japes"},
    {A11Y_St_Kind_Shrine, "Hyrule Temple"},
    {A11Y_St_Kind_Yoster, "Yoshi's Island"},
    {A11Y_St_Kind_Greens, "Green Greens"},
    {A11Y_St_Kind_Venom, "Venom"},

    {A11Y_St_Kind_Zebes, "Brinstar"},
    {A11Y_St_Kind_Onett, "Onett"},
    {A11Y_St_Kind_MuteCity, "Mute City"},
    {A11Y_St_Kind_PStadium, "Pokémon Stadium"},
    {A11Y_St_Kind_Inishie1, "Mushroom Kingdom"},

    {A11Y_St_Kind_Kraid, "Brinstar Depths"},
    {A11Y_St_Kind_Fourside, "Fourside"},
    {A11Y_St_Kind_BigBlue, "Big Blue"},
    {A11Y_St_Kind_Pura, "Poké Floats"},
    {A11Y_St_Kind_Inishie2, "Mushroom Kingdom II"},

    {A11Y_St_Kind_Battle, "Battlefield"},
    {A11Y_St_Kind_Last, "Final Destination"},
    {A11Y_St_Kind_OldPupupu, "Dream Land N64"},
    {A11Y_St_Kind_OldYoshi, "Yoshi's Island N64"},
    {A11Y_St_Kind_OldKongo, "Kongo Jungle N64"},
};

constexpr A11yStage kAllStages[] = {
#define A11Y_STAGE_ITEM(name, number) A11Y_##name,
    A11Y_STAGE_KINDS(A11Y_STAGE_ITEM)
#undef A11Y_STAGE_ITEM
};

constexpr bool table_has_every_stage_once() {
    for (A11yStage stage : kAllStages) {
        int found = 0;
        for (const StageWords& words : kStages) {
            found += words.stage == stage ? 1 : 0;
        }
        if (found != 1) {
            return false;
        }
    }
    return std::size(kStages) == std::size(kAllStages);
}
static_assert(table_has_every_stage_once(), "kStages needs every stage kind, each once");

}  // namespace

std::optional<std::string_view> stage_name(A11yStage stage) {
    for (const StageWords& words : kStages) {
        if (words.stage == stage) {
            return words.name;
        }
    }
    return std::nullopt;
}

}  // namespace a11y
