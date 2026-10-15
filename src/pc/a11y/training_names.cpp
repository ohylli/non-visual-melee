/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "training_names.hpp"
#include "game_access.h"
#include "item_names.hpp"
#include <iterator>
#include <span>

namespace a11y {
namespace {

struct NumberedRow {
    int number;
    TrainingRow row;
};

/* The rows top to bottom, as drawn on the left of the screen. The hidden row
 * between CP Damage and Camera has no name: the cursor skips it. */
constexpr NumberedRow kRows[] = {
    {A11Y_TRAINING_SPEED, {"Speed", ": ", TrainingValue::speed}},
    {A11Y_TRAINING_ITEM, {"Item", ": ", TrainingValue::item}},
    {A11Y_TRAINING_CPU_COUNT, {"No. of CPs", ", ", TrainingValue::cpu_count}},
    {A11Y_TRAINING_CPU_BEHAVIOUR, {"CP", ", ", TrainingValue::cpu_behaviour}},
    {A11Y_TRAINING_CPU_DAMAGE, {"CP Damage", ": ", TrainingValue::cpu_damage}},
    {A11Y_TRAINING_CAMERA, {"Camera", ": ", TrainingValue::camera}},
    {A11Y_TRAINING_RESET, {"Reset", "", TrainingValue::none}},
    {A11Y_TRAINING_FINISH, {"Finish", "", TrainingValue::none}},
};

/* The game speed, fastest first, as gm_1884.c's speeds (lbl_803B7C68): 2,
 * 1.5, 1, 2/3, 1/2 and 1/4 of normal. */
constexpr std::string_view kSpeedWords[] = {"x2", "x1.5", "x1", "2/3", "1/2", "1/4"};
constexpr std::string_view kCpuBehaviourWords[] = {
    "Stand", "Walk", "Evade", "Jump", "Attack", "Human"};
constexpr std::string_view kCameraWords[] = {"Normal", "Zoom", "Free"};
/* The menu keeps one less than the number it draws and applies. */
constexpr int kMostCpus = 3;
constexpr int kMostDamage = 999;

/* A copy of the kinds in gmTraining_ItemTable (static in gm_1884.c), the
 * menu's item order, which its value indexes. The table's 31st entry, no
 * item, is out of the cursor's reach. */
constexpr A11yItem kItems[] = {
    A11Y_It_Kind_Foods,
    A11Y_It_Kind_Tomato,
    A11Y_It_Kind_Heart,
    A11Y_It_Kind_L_Gun,
    A11Y_It_Kind_F_Flower,
    A11Y_It_Kind_S_Scope,
    A11Y_It_Kind_StarRod,
    A11Y_It_Kind_LipStick,
    A11Y_It_Kind_Sword,
    A11Y_It_Kind_Bat,
    A11Y_It_Kind_Harisen,
    A11Y_It_Kind_Hammer,
    A11Y_It_Kind_G_Shell,
    A11Y_It_Kind_R_Shell,
    A11Y_It_Kind_Flipper,
    A11Y_It_Kind_Freeze,
    A11Y_It_Kind_Dosei,
    A11Y_It_Kind_M_Ball,
    A11Y_It_Kind_BombHei,
    A11Y_It_Kind_MSBomb,
    A11Y_It_Kind_Kinoko,
    A11Y_It_Kind_DKinoko,
    A11Y_It_Kind_Star,
    A11Y_It_Kind_Parasol,
    A11Y_It_Kind_WStar,
    A11Y_It_Kind_MetalB,
    A11Y_It_Kind_RabbitC,
    A11Y_It_Kind_ScBall,
    A11Y_It_Kind_TaruCann,
    A11Y_It_Kind_Spycloak,
};

std::optional<std::string> word(std::span<const std::string_view> words, int number) {
    if (number < 0 || number >= static_cast<int>(words.size())) {
        return std::nullopt;
    }
    return std::string(words[static_cast<std::size_t>(number)]);
}

}  // namespace

const TrainingRow* training_row(int row) {
    for (const NumberedRow& numbered : kRows) {
        if (numbered.number == row) {
            return &numbered.row;
        }
    }
    return nullptr;
}

std::optional<std::string> training_value_words(TrainingValue value, int number) {
    switch (value) {
    case TrainingValue::speed:
        return word(kSpeedWords, number);
    case TrainingValue::item:
        if (std::optional<std::string_view> name = item_name(training_item(number))) {
            return std::string(*name);
        }
        return std::nullopt;
    case TrainingValue::cpu_count:
        if (number < 0 || number >= kMostCpus) {
            return std::nullopt;
        }
        return std::to_string(number + 1);
    case TrainingValue::cpu_behaviour:
        return word(kCpuBehaviourWords, number);
    case TrainingValue::cpu_damage:
        if (number < 0 || number > kMostDamage) {
            return std::nullopt;
        }
        return std::to_string(number);
    case TrainingValue::camera:
        return word(kCameraWords, number);
    case TrainingValue::none:
        break;
    }
    return std::nullopt;
}

A11yItem training_item(int index) {
    if (index < 0 || index >= static_cast<int>(std::size(kItems))) {
        return A11Y_NO_ITEM;
    }
    return kItems[index];
}

}  // namespace a11y
