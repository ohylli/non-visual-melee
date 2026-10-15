/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The items' names, as Item Switch draws them on the English screen, keyed by
 * the item rather than by any screen's order: Item Switch and the Training
 * menu list the same items in different orders, and an item is named the
 * same on both (.scratch/training-menu/spec.md, "Speech"). */
#pragma once
#include "item_kinds.h"
#include <optional>
#include <string_view>

namespace a11y {

/* An item's name; nothing for none or a kind missing from the table. */
std::optional<std::string_view> item_name(A11yItem item);

inline constexpr std::string_view kUnknownItem = "Unknown item";

}  // namespace a11y
