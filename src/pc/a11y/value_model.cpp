/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "value_model.hpp"
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

std::string format_amount(const NamedValue& named, float value) {
    if (named.kind == ValueKind::toggle) {
        return value != 0.0f ? "on" : "off";
    }
    std::string number = format_number(value);
    if (named.unit.empty()) {
        return number;
    }
    return named.unit == "%" ? number + "%" : number + " " + named.unit;
}

/* The comment above a value: its description, then what it takes. */
std::string describe(const NamedValue& named) {
    std::string range = named.kind == ValueKind::toggle ?
                            "On or off" :
                            format_number(named.min) + " to " + format_amount(named, named.max);
    return "# " + named.description + " " + range + ", default " +
           format_amount(named, named.default_value) + ".\n";
}

std::string format_line(const NamedValue& named, float value) {
    std::string text =
        named.kind == ValueKind::toggle ? format_amount(named, value) : format_number(value);
    return named.key + " = " + text + "\n";
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
bool read_number(const NamedValue& named, std::string_view text, float& out) {
    std::string number = lowered(text);
    std::string unit = lowered(named.unit);
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

std::string format_value(const NamedValue& named) {
    return named.kind == ValueKind::toggle ? format_amount(named, named.value) :
                                             format_number(named.value);
}

Switch ValueGroup::add_switch(const SwitchSpec& spec) {
    NamedValue named;
    named.key = spec.name;
    named.label = spec.label;
    named.description = spec.description;
    named.kind = ValueKind::toggle;
    named.default_value = spec.default_on ? 1.0f : 0.0f;
    named.min = 0.0f;
    named.max = 1.0f;
    named.step = 1.0f;
    return Switch(m_model->add(m_index, std::move(named)));
}

Number ValueGroup::add_number(const NumberSpec& spec) {
    NamedValue named;
    named.key = spec.name;
    named.label = spec.label;
    named.description = spec.description;
    named.unit = spec.unit;
    named.kind = ValueKind::number;
    named.min = spec.min;
    named.max = spec.max;
    named.default_value = std::clamp(spec.default_value, spec.min, spec.max);
    named.step = spec.step;
    return Number(m_model->add(m_index, std::move(named)));
}

ValueGroup ValueModel::group(std::string_view id, std::string_view title) {
    for (std::size_t i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].id == id) {
            return ValueGroup(*this, i);
        }
    }
    m_groups.push_back(Group{std::string(id), std::string(title), {}});
    return ValueGroup(*this, m_groups.size() - 1);
}

NamedValue& ValueModel::add(std::size_t group, NamedValue named) {
    Group& owner = m_groups[group];
    named.key = owner.id + "." + named.key;
    named.value = named.default_value;
    /* A key registered twice is a programming error; the second gets the
     * first's value rather than a duplicate line in the file. */
    if (NamedValue* existing = find_mutable(named.key)) {
        return *existing;
    }
    NamedValue& added = m_values.emplace_back(std::move(named));
    owner.values.push_back(&added);
    return added;
}

NamedValue* ValueModel::find_mutable(std::string_view key) {
    for (NamedValue& named : m_values) {
        if (named.key == key) {
            return &named;
        }
    }
    return nullptr;
}

const NamedValue* ValueModel::find(std::string_view key) const {
    for (const NamedValue& named : m_values) {
        if (named.key == key) {
            return &named;
        }
    }
    return nullptr;
}

bool ValueModel::set(const NamedValue& named, float value) {
    NamedValue* target = find_mutable(named.key);
    if (target == nullptr || !std::isfinite(value)) {
        return false;
    }
    value = std::clamp(value, target->min, target->max);
    if (target->kind == ValueKind::toggle) {
        value = value != 0.0f ? 1.0f : 0.0f;
    }
    if (value == target->value) {
        return false;
    }
    target->value = value;
    return true;
}

std::string ValueModel::to_text(bool at_defaults) const {
    std::vector<const NamedValue*> all;
    for (const NamedValue& named : m_values) {
        all.push_back(&named);
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

std::string ValueModel::to_text(const std::vector<const NamedValue*>& only) const {
    return write(only, false);
}

std::string ValueModel::write(const std::vector<const NamedValue*>& only, bool at_defaults) const {
    std::string text;
    for (const Group& group : m_groups) {
        bool heading = false;
        for (const NamedValue* named : group.values) {
            if (std::find(only.begin(), only.end(), named) == only.end()) {
                continue;
            }
            if (!heading) {
                text += "\n# " + group.title + "\n";
                heading = true;
            }
            float value = at_defaults ? named->default_value : named->value;
            text += "\n" + describe(*named) + format_line(*named, value);
        }
    }
    return text;
}

TextRead ValueModel::read_text(std::string_view text) {
    TextRead read;
    /* The value each known key's line gives, or the value it held when the
     * line cannot be read. */
    std::map<NamedValue*, float> given;
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
        NamedValue* named = equals == std::string_view::npos ? nullptr : find_mutable(key);
        if (equals == std::string_view::npos || key.empty()) {
            read.bad_lines.push_back(number);
            read.notes.push_back(
                "line " + std::to_string(number) + " not understood: " + std::string(trim(raw)));
            continue;
        }
        if (named == nullptr) {
            unknown[std::string(key)] = std::string(value);
            read.unknown.emplace_back(key);
            continue;
        }
        float parsed = 0.0f;
        bool ok = named->kind == ValueKind::toggle ? read_toggle(value, parsed) :
                                                     read_number(*named, value, parsed);
        if (!ok) {
            read.bad_lines.push_back(number);
            read.notes.push_back("line " + std::to_string(number) +
                                 " not understood: " + std::string(trim(raw)) + "; " + named->key +
                                 " stays " + format_amount(*named, named->value));
            given.try_emplace(named, named->value);
            continue;
        }
        float clamped = std::clamp(parsed, named->min, named->max);
        if (clamped != parsed) {
            read.notes.push_back("line " + std::to_string(number) + ": " + named->key + " " +
                                 format_number(parsed) + " is out of range, using " +
                                 format_amount(*named, clamped));
        }
        given[named] = clamped;
    }
    for (NamedValue& named : m_values) {
        auto found = given.find(&named);
        float value = named.default_value;
        if (found != given.end()) {
            value = found->second;
        } else {
            read.missing.push_back(&named);
        }
        if (value != named.value) {
            named.value = value;
            read.changed.push_back(&named);
        }
    }
    m_unknown = std::move(unknown);
    return read;
}

}  // namespace a11y
