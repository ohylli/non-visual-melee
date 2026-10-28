/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The words for a voice's language on the Voice rows (voice_settings.hpp):
 * OneCore gives a language tag ("en-US"), and the rows say it as Windows
 * writes it in English ("English (United States)"). A fork string table of
 * the languages and regions OneCore voices come in; a tag it lacks is said
 * as written. */
#pragma once
#include <string>
#include <string_view>

namespace a11y {

/* "en-US" as "English (United States)", "fi" as "Finnish"; a language or
 * region the table lacks gives the tag as written. */
std::string language_name(std::string_view tag);

}  // namespace a11y
