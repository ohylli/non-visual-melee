/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "voice_names.hpp"
#include <array>
#include <utility>

namespace a11y {
namespace {

using Name = std::pair<std::string_view, std::string_view>;

/* The languages of Windows' OneCore voices, by their tags' first part, as
 * Windows names them in English. */
constexpr std::array kLanguages = {
    Name{"ar", "Arabic"},
    Name{"bg", "Bulgarian"},
    Name{"ca", "Catalan"},
    Name{"cs", "Czech"},
    Name{"da", "Danish"},
    Name{"de", "German"},
    Name{"el", "Greek"},
    Name{"en", "English"},
    Name{"es", "Spanish"},
    Name{"fi", "Finnish"},
    Name{"fr", "French"},
    Name{"he", "Hebrew"},
    Name{"hi", "Hindi"},
    Name{"hr", "Croatian"},
    Name{"hu", "Hungarian"},
    Name{"id", "Indonesian"},
    Name{"it", "Italian"},
    Name{"ja", "Japanese"},
    Name{"ko", "Korean"},
    Name{"ms", "Malay"},
    Name{"nb", "Norwegian Bokm\xC3\xA5l"},
    Name{"nl", "Dutch"},
    Name{"pl", "Polish"},
    Name{"pt", "Portuguese"},
    Name{"ro", "Romanian"},
    Name{"ru", "Russian"},
    Name{"sk", "Slovak"},
    Name{"sl", "Slovenian"},
    Name{"sv", "Swedish"},
    Name{"ta", "Tamil"},
    Name{"th", "Thai"},
    Name{"tr", "Turkish"},
    Name{"uk", "Ukrainian"},
    Name{"vi", "Vietnamese"},
    Name{"zh", "Chinese"},
};

/* Their regions, by the tags' second part. */
constexpr std::array kRegions = {
    Name{"AT", "Austria"},
    Name{"AU", "Australia"},
    Name{"BE", "Belgium"},
    Name{"BG", "Bulgaria"},
    Name{"BR", "Brazil"},
    Name{"CA", "Canada"},
    Name{"CH", "Switzerland"},
    Name{"CN", "China"},
    Name{"CZ", "Czechia"},
    Name{"DE", "Germany"},
    Name{"DK", "Denmark"},
    Name{"EG", "Egypt"},
    Name{"ES", "Spain"},
    Name{"FI", "Finland"},
    Name{"FR", "France"},
    Name{"GB", "United Kingdom"},
    Name{"GR", "Greece"},
    Name{"HK", "Hong Kong SAR"},
    Name{"HR", "Croatia"},
    Name{"HU", "Hungary"},
    Name{"ID", "Indonesia"},
    Name{"IE", "Ireland"},
    Name{"IL", "Israel"},
    Name{"IN", "India"},
    Name{"IT", "Italy"},
    Name{"JP", "Japan"},
    Name{"KR", "Korea"},
    Name{"MX", "Mexico"},
    Name{"MY", "Malaysia"},
    Name{"NL", "Netherlands"},
    Name{"NO", "Norway"},
    Name{"PL", "Poland"},
    Name{"PT", "Portugal"},
    Name{"RO", "Romania"},
    Name{"RU", "Russia"},
    Name{"SA", "Saudi Arabia"},
    Name{"SE", "Sweden"},
    Name{"SI", "Slovenia"},
    Name{"SK", "Slovakia"},
    Name{"TH", "Thailand"},
    Name{"TR", "Turkey"},
    Name{"TW", "Taiwan"},
    Name{"UA", "Ukraine"},
    Name{"US", "United States"},
    Name{"VN", "Vietnam"},
};

template <class Table>
std::string_view look_up(const Table& table, std::string_view key) {
    for (const Name& name : table) {
        if (name.first == key) {
            return name.second;
        }
    }
    return {};
}

}  // namespace

std::string language_name(std::string_view tag) {
    const std::size_t dash = tag.find('-');
    const std::string_view language = look_up(kLanguages, tag.substr(0, dash));
    if (language.empty()) {
        return std::string(tag);
    }
    if (dash == std::string_view::npos) {
        return std::string(language);
    }
    const std::string_view region = look_up(kRegions, tag.substr(dash + 1));
    if (region.empty()) {
        return std::string(tag);
    }
    return std::string(language) + " (" + std::string(region) + ")";
}

}  // namespace a11y
