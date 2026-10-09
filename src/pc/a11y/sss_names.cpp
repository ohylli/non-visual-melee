/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sss_names.hpp"
#include <iterator>

namespace a11y {
namespace {

struct StageWords {
    A11yStage stage;
    std::string_view name;
    std::string_view description;
};

/* Every stage on the grid, each once, in the order of the tiles: Icicle
 * Mountain at the far left, the two rows of stacked pairs (the top row, then
 * the row under it), Flat Zone at the far right, the block beside the random
 * tile and the locked row under it, then the smaller bottom row. */
constexpr StageWords kStages[] = {
    {A11Y_St_Kind_Icemt, "Icicle Mountain",
        "A climb up an icy mountain that scrolls as you fight, over ledges of rock and ice at many "
        "heights."},
    {A11Y_St_Kind_Castle, "Princess Peach's Castle",
        "The rooftops of a castle above its moat, at several heights around a tower in the middle. "
        "Two platforms rise when stood on and sink back when left."},
    {A11Y_St_Kind_Kongo, "Kongo Jungle",
        "Wooden platforms in the treetops above a waterfall. A moving barrel catches fighters and "
        "fires them out, and Klaptraps appear now and then."},
    {A11Y_St_Kind_GreatBay, "Great Bay",
        "A wooden pier over the sea, beside a turtle that dives and surfaces elsewhere as a "
        "platform. Tingle floats above on a balloon that tilts and sinks when stood on."},
    {A11Y_St_Kind_Story, "Yoshi's Story",
        "A small island with three floating platforms and a cloud platform that drifts by. Shy "
        "Guys fly past carrying food."},
    {A11Y_St_Kind_Izumi, "Fountain of Dreams",
        "A round stage over a fountain, with a platform above it and two side platforms that rise "
        "and sink."},
    {A11Y_St_Kind_Corneria, "Corneria",
        "A fight on the hull of a big ship flying over a city. Arwings fly past and fire, and so "
        "does the ship's cannon."},
    {A11Y_St_Kind_Flatzone, "Flat Zone",
        "A flat Game & Watch screen whose simple platforms change now and then. Tools fall from "
        "above."},

    {A11Y_St_Kind_RCruise, "Rainbow Cruise",
        "A flying ship sailing through the sky, with platforms around it. The stage scrolls as the "
        "fight goes on."},
    {A11Y_St_Kind_Garden, "Jungle Japes",
        "A wooden hut over a river whose current carries fallen fighters away. A Klaptrap leaps "
        "out of the water now and then."},
    {A11Y_St_Kind_Shrine, "Hyrule Temple",
        "A big temple on floating islands, with ground at many heights and lots of room to run."},
    {A11Y_St_Kind_Yoster, "Yoshi's Island",
        "Grassy hills on either side of a row of blocks that spin and can be passed through for a "
        "while when hit or landed on hard. A pipe stands at one end."},
    {A11Y_St_Kind_Greens, "Green Greens",
        "An island with Whispy Woods, a tree that blows wind and drops apples, in the middle. "
        "Blocks fall and stack up, and some of them are bombs."},
    {A11Y_St_Kind_Venom, "Venom",
        "A fight on top of a big ship flying through a dark red sky. Arwings fly through and "
        "fire."},

    {A11Y_St_Kind_Zebes, "Brinstar",
        "A small alien stage of several pieces over acid that rises and falls and burns. Some "
        "pieces break when hit and grow back."},
    {A11Y_St_Kind_Onett, "Onett",
        "A town street in front of buildings, with springy awnings to land on that give way after "
        "a while. Cars race down the road and hit anyone in their way."},
    {A11Y_St_Kind_MuteCity, "Mute City",
        "Platforms flying over a race track, where race cars speed past and can hit fighters."},
    {A11Y_St_Kind_PStadium, "Pokémon Stadium",
        "A wide field with two platforms that transforms into fire, grass, water and rock layouts "
        "and back."},
    {A11Y_St_Kind_Inishie1, "Mushroom Kingdom",
        "Brick ground with gaps between, from the first Super Mario Bros., with question blocks "
        "that give items. Two lifts on a scale tip under weight and fall if pushed too far."},

    {A11Y_St_Kind_Kraid, "Brinstar Depths", "A rock that Kraid, a giant monster, tilts and turns."},
    {A11Y_St_Kind_Fourside, "Fourside",
        "City rooftops at night at several heights. A UFO stops above for a while as a platform, "
        "and a crane raises and lowers an iron beam."},
    {A11Y_St_Kind_BigBlue, "Big Blue",
        "A race at high speed: fighters stand on cars and floating platforms above a track racing "
        "past below."},
    {A11Y_St_Kind_Pura, "Poké Floats",
        "Giant Pokémon balloons float past one after another, and the fight moves from one to the "
        "next."},
    {A11Y_St_Kind_Inishie2, "Mushroom Kingdom II",
        "Two grassy ledges over a waterfall, with a flying carpet drifting over the water. Birdo "
        "appears now and then and fires eggs."},

    {A11Y_St_Kind_Battle, "Battlefield",
        "One floating stage with three platforms above it, two low at the sides and one higher in "
        "the middle. Nothing moves."},
    {A11Y_St_Kind_Last, "Final Destination",
        "One long, flat stage with no platforms, floating in space. Nothing moves."},
    {A11Y_St_Kind_OldPupupu, "Dream Land N64",
        "An island stage where Whispy Woods, a tree, blows wind across the fight."},
    {A11Y_St_Kind_OldYoshi, "Yoshi's Island N64",
        "A hilly stage under an orange sky, with cloud platforms that sink when stood on and "
        "vanish for a while."},
    {A11Y_St_Kind_OldKongo, "Kongo Jungle N64",
        "A jungle stage at sunset, with a moving barrel that catches fighters and fires them out."},
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

const StageWords* find(A11yStage stage) {
    for (const StageWords& words : kStages) {
        if (words.stage == stage) {
            return &words;
        }
    }
    return nullptr;
}

}  // namespace

std::optional<std::string_view> stage_name(A11yStage stage) {
    if (const StageWords* words = find(stage)) {
        return words->name;
    }
    return std::nullopt;
}

std::optional<std::string_view> stage_description(A11yStage stage) {
    if (const StageWords* words = find(stage)) {
        return words->description;
    }
    return std::nullopt;
}

}  // namespace a11y
