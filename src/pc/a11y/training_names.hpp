/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The Training menu's string table: its rows' names and their values' words,
 * as the English screen draws them (.scratch/training-menu/spec.md,
 * "Speech"). The game draws them as pictures, all but the chosen item's name,
 * which is game text but named here from the items' table (item_names.hpp)
 * so an item sounds the same as on Item Switch. */
#pragma once
#include "item_kinds.h"
#include <optional>
#include <string>
#include <string_view>

namespace a11y {

/* What a row holds beside its name. */
enum class TrainingValue {
    /* Reset and Finish: an action A carries out. */
    none,
    speed,
    item,
    cpu_count,
    cpu_behaviour,
    cpu_damage,
    camera,
};

struct TrainingRow {
    std::string_view name;
    /* Between the name and the value: ": ", or ", " where the drawn label's
     * own punctuation would read badly beside a colon ("No. of CPs, 2",
     * "CP, Stand"). */
    std::string_view separator;
    TrainingValue value;
};

/* A row by the cursor's number (A11yTrainingMenu.row); nothing for the
 * hidden row or any number outside the menu. */
const TrainingRow* training_row(int row);

/* A value's words as drawn ("x1.5", "Maxim Tomato", "2", "Walk", "57",
 * "Zoom"), by the number the menu keeps; nothing for a number outside the
 * row's range, or for a row with no value. */
std::optional<std::string> training_value_words(TrainingValue value, int number);

/* The item at a place in the Training menu's own item order; none for a
 * place outside it. */
A11yItem training_item(int index);

/* The opening: "Training menu. Speed: x1.", then on the first opening of a
 * Training session the hint. */
inline constexpr std::string_view kTrainingMenu = "Training menu";
inline constexpr std::string_view kTrainingHint =
    "Up and down for rows, left and right to adjust, A on Item, Reset and Finish, Start to "
    "close.";

}  // namespace a11y
