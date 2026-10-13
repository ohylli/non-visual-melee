/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <utility>

namespace a11y {
namespace {

std::string_view trim(std::string_view text) {
    const char* space = " \t\r\n";
    std::size_t first = text.find_first_not_of(space);
    if (first == std::string_view::npos) {
        return {};
    }
    std::size_t last = text.find_last_not_of(space);
    return text.substr(first, last - first + 1);
}

std::string lowered(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

/* The shortest text that reads back as the same float, whatever the
 * locale. */
std::string format_number(float value) {
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

std::string format_amount(const Setting& setting, float value) {
    if (setting.kind == SettingKind::toggle) {
        return value != 0.0f ? "on" : "off";
    }
    std::string number = format_number(value);
    if (setting.unit.empty()) {
        return number;
    }
    return setting.unit == "%" ? number + "%" : number + " " + setting.unit;
}

/* The comment above a value: its description, then what it takes. */
std::string describe(const Setting& setting) {
    std::string range =
        setting.kind == SettingKind::toggle ?
            "On or off" :
            format_number(setting.min) + " to " + format_amount(setting, setting.max);
    return "# " + setting.description + " " + range + ", default " +
           format_amount(setting, setting.default_value) + ".\n";
}

std::string format_line(const Setting& setting, float value) {
    std::string text =
        setting.kind == SettingKind::toggle ? format_amount(setting, value) : format_number(value);
    return setting.key + " = " + text + "\n";
}

bool read_toggle(std::string_view text, float& out) {
    std::string word = lowered(text);
    if (word == "on" || word == "true" || word == "yes" || word == "1") {
        out = 1.0f;
        return true;
    }
    if (word == "off" || word == "false" || word == "no" || word == "0") {
        out = 0.0f;
        return true;
    }
    return false;
}

/* A number, optionally followed by the value's unit ("80%", "0.5 s"). A
 * decimal comma is taken for a point. */
bool read_number(const Setting& setting, std::string_view text, float& out) {
    std::string number = lowered(text);
    std::string unit = lowered(setting.unit);
    if (!unit.empty() && number.size() > unit.size() &&
        number.compare(number.size() - unit.size(), unit.size(), unit) == 0)
    {
        number = std::string(trim(std::string_view(number).substr(0, number.size() - unit.size())));
    }
    std::replace(number.begin(), number.end(), ',', '.');
    std::size_t start = !number.empty() && number[0] == '+' ? 1 : 0;
    const char* first = number.data() + start;
    const char* last = number.data() + number.size();
    float value = 0.0f;
    auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc() || result.ptr != last || first == last || !std::isfinite(value)) {
        return false;
    }
    out = value;
    return true;
}

}  // namespace

std::string format_value(const Setting& setting) {
    return setting.kind == SettingKind::toggle ? format_amount(setting, setting.value) :
                                                 format_number(setting.value);
}

Switch SettingsGroup::add_switch(const SwitchSpec& spec) {
    Setting setting;
    setting.key = spec.name;
    setting.label = spec.label;
    setting.description = spec.description;
    setting.kind = SettingKind::toggle;
    setting.default_value = spec.default_on ? 1.0f : 0.0f;
    setting.min = 0.0f;
    setting.max = 1.0f;
    setting.step = 1.0f;
    return Switch(m_settings->add(m_index, std::move(setting)));
}

Number SettingsGroup::add_number(const NumberSpec& spec) {
    Setting setting;
    setting.key = spec.name;
    setting.label = spec.label;
    setting.description = spec.description;
    setting.unit = spec.unit;
    setting.kind = SettingKind::number;
    setting.min = spec.min;
    setting.max = spec.max;
    setting.default_value = std::clamp(spec.default_value, spec.min, spec.max);
    setting.step = spec.step;
    return Number(m_settings->add(m_index, std::move(setting)));
}

SettingsGroup Settings::group(std::string_view id, std::string_view title) {
    for (std::size_t i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].id == id) {
            return SettingsGroup(*this, i);
        }
    }
    m_groups.push_back(Group{std::string(id), std::string(title), {}});
    return SettingsGroup(*this, m_groups.size() - 1);
}

Setting& Settings::add(std::size_t group, Setting setting) {
    Group& owner = m_groups[group];
    setting.key = owner.id + "." + setting.key;
    setting.value = setting.default_value;
    /* A key registered twice is a programming error; the second gets the
     * first's value rather than a duplicate line in the file. */
    if (Setting* existing = find_mutable(setting.key)) {
        return *existing;
    }
    Setting& added = m_settings.emplace_back(std::move(setting));
    owner.settings.push_back(&added);
    return added;
}

Setting* Settings::find_mutable(std::string_view key) {
    for (Setting& setting : m_settings) {
        if (setting.key == key) {
            return &setting;
        }
    }
    return nullptr;
}

const Setting* Settings::find(std::string_view key) const {
    for (const Setting& setting : m_settings) {
        if (setting.key == key) {
            return &setting;
        }
    }
    return nullptr;
}

bool Settings::set(const Setting& setting, float value) {
    Setting* target = find_mutable(setting.key);
    if (target == nullptr || !std::isfinite(value)) {
        return false;
    }
    value = std::clamp(value, target->min, target->max);
    if (target->kind == SettingKind::toggle) {
        value = value != 0.0f ? 1.0f : 0.0f;
    }
    if (value == target->value) {
        return false;
    }
    target->value = value;
    return true;
}

std::string Settings::to_text(bool at_defaults) const {
    std::vector<const Setting*> all;
    for (const Setting& setting : m_settings) {
        all.push_back(&setting);
    }
    std::string text = write(all, at_defaults);
    if (!m_unknown.empty()) {
        text += "\n# Not known to this version of the game, kept as written\n\n";
        for (const auto& [key, value] : m_unknown) {
            text += key + " = " + value + "\n";
        }
    }
    return text;
}

std::string Settings::to_text(const std::vector<const Setting*>& only) const {
    return write(only, false);
}

std::string Settings::write(const std::vector<const Setting*>& only, bool at_defaults) const {
    std::string text;
    for (const Group& group : m_groups) {
        bool heading = false;
        for (const Setting* setting : group.settings) {
            if (std::find(only.begin(), only.end(), setting) == only.end()) {
                continue;
            }
            if (!heading) {
                text += "\n# " + group.title + "\n";
                heading = true;
            }
            float value = at_defaults ? setting->default_value : setting->value;
            text += "\n" + describe(*setting) + format_line(*setting, value);
        }
    }
    return text;
}

SettingsRead Settings::read_text(std::string_view text) {
    SettingsRead read;
    /* The value each known key's line gives, or its last setting when the
     * line cannot be read. */
    std::map<Setting*, float> given;
    std::map<std::string, std::string> unknown;
    /* Notepad may save with a byte order mark. */
    if (text.substr(0, 3) == "\xEF\xBB\xBF") {
        text.remove_prefix(3);
    }
    int number = 0;
    while (!text.empty() || number == 0) {
        ++number;
        std::size_t end = text.find('\n');
        std::string_view raw = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
        std::string_view line = trim(raw.substr(0, raw.find('#')));
        if (line.empty()) {
            continue;
        }
        std::size_t equals = line.find('=');
        std::string_view key =
            equals == std::string_view::npos ? line : trim(line.substr(0, equals));
        std::string_view value =
            equals == std::string_view::npos ? std::string_view() : trim(line.substr(equals + 1));
        Setting* setting = equals == std::string_view::npos ? nullptr : find_mutable(key);
        if (equals == std::string_view::npos || key.empty()) {
            read.bad_lines.push_back(number);
            read.notes.push_back(
                "line " + std::to_string(number) + " not understood: " + std::string(trim(raw)));
            continue;
        }
        if (setting == nullptr) {
            unknown[std::string(key)] = std::string(value);
            read.unknown.emplace_back(key);
            continue;
        }
        float parsed = 0.0f;
        bool ok = setting->kind == SettingKind::toggle ? read_toggle(value, parsed) :
                                                         read_number(*setting, value, parsed);
        if (!ok) {
            read.bad_lines.push_back(number);
            read.notes.push_back(
                "line " + std::to_string(number) + " not understood: " + std::string(trim(raw)) +
                "; " + setting->key + " stays " + format_amount(*setting, setting->value));
            given.try_emplace(setting, setting->value);
            continue;
        }
        float clamped = std::clamp(parsed, setting->min, setting->max);
        if (clamped != parsed) {
            read.notes.push_back("line " + std::to_string(number) + ": " + setting->key + " " +
                                 format_number(parsed) + " is out of range, using " +
                                 format_amount(*setting, clamped));
        }
        given[setting] = clamped;
    }
    for (Setting& setting : m_settings) {
        auto found = given.find(&setting);
        float value = setting.default_value;
        if (found != given.end()) {
            value = found->second;
        } else {
            read.missing.push_back(&setting);
        }
        if (value != setting.value) {
            setting.value = value;
            read.changed.push_back(&setting);
        }
    }
    m_unknown = std::move(unknown);
    return read;
}

}  // namespace a11y
