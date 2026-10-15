/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "item_names.hpp"

namespace a11y {
namespace {

struct ItemWords {
    A11yItem item;
    std::string_view name;
};

/* In Item Switch's order: the left column, then the right. The Motion-Sensor
 * Bomb is its US English name: the Training menu's code swaps that text in
 * for US English only (fn_80188EE8), the decomp calling it a Proximity Mine. */
constexpr ItemWords kItems[] = {
    {A11Y_It_Kind_Foods, "Food"},
    {A11Y_It_Kind_Tomato, "Maxim Tomato"},
    {A11Y_It_Kind_Heart, "Heart Container"},
    {A11Y_It_Kind_WStar, "Warp Star"},
    {A11Y_It_Kind_L_Gun, "Ray Gun"},
    {A11Y_It_Kind_S_Scope, "Super Scope"},
    {A11Y_It_Kind_F_Flower, "Fire Flower"},
    {A11Y_It_Kind_LipStick, "Lip's Stick"},
    {A11Y_It_Kind_StarRod, "Star Rod"},
    {A11Y_It_Kind_Sword, "Beam Sword"},
    {A11Y_It_Kind_Bat, "Home-Run Bat"},
    {A11Y_It_Kind_Harisen, "Fan"},
    {A11Y_It_Kind_Hammer, "Hammer"},
    {A11Y_It_Kind_G_Shell, "Green Shell"},
    {A11Y_It_Kind_R_Shell, "Red Shell"},
    {A11Y_It_Kind_Flipper, "Flipper"},
    {A11Y_It_Kind_Freeze, "Freezie"},
    {A11Y_It_Kind_Dosei, "Mr. Saturn"},
    {A11Y_It_Kind_M_Ball, "Poké Ball"},
    {A11Y_It_Kind_BombHei, "Bob-omb"},
    {A11Y_It_Kind_MSBomb, "Motion-Sensor Bomb"},
    {A11Y_It_Kind_Kinoko, "Super Mushroom"},
    {A11Y_It_Kind_DKinoko, "Poison Mushroom"},
    {A11Y_It_Kind_Star, "Starman"},
    {A11Y_It_Kind_Parasol, "Parasol"},
    {A11Y_It_Kind_ScBall, "Screw Attack"},
    {A11Y_It_Kind_MetalB, "Metal Box"},
    {A11Y_It_Kind_RabbitC, "Bunny Hood"},
    {A11Y_It_Kind_Spycloak, "Cloaking Device"},
    {A11Y_It_Kind_TaruCann, "Barrel Cannon"},
    {A11Y_It_Kind_Kusudama, "Party Ball"},
};

}  // namespace

std::optional<std::string_view> item_name(A11yItem item) {
    for (const ItemWords& words : kItems) {
        if (words.item == item) {
            return words.name;
        }
    }
    return std::nullopt;
}

}  // namespace a11y
