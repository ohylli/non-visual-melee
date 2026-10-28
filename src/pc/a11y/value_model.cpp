/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "value_model.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <utility>

namespace a11y {
namespace {

/* Heads the keys a text had that the model does not, when the whole model
 * is written. */
constexpr std::string_view kUnknownTitle = "Not known to this version of the game, kept as written";

std::string_view trim(std::string_view text) {
    const char* space = " \t\r\n";
    std::size_t first = text.find_first_not_of(space);
    if (first == std::string_view::npos) {
        return {};
    }
    std::size_t last = text.find_last_not_of(space);
    return text.substr(first, last - first + 1);
}

/* A line without its ending. */
std::string_view line_body(std::string_view line) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.remove_suffix(1);
    }
    return line;
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

/* A choice's fixed options as the file writes them: "Off, Every change or
 * Thresholds". Options asked at run time are left unnamed, so the file
 * never depends on what was installed when it was written. */
std::string option_list(const NamedValue& named) {
    if (named.list || named.options.empty()) {
        return "One of the options the menus list";
    }
    std::string text;
    for (std::size_t i = 0; i < named.options.size(); ++i) {
        if (i > 0) {
            text += i + 1 == named.options.size() ? " or " : ", ";
        }
        text += named.options[i].text;
    }
    return text;
}

/* The comment above a value: its description, then what it takes. */
std::string describe(const NamedValue& named) {
    std::string range;
    switch (named.kind) {
    case ValueKind::toggle:
        range = "On or off";
        break;
    case ValueKind::number:
        range = format_number(named.min) + " to " + format_amount(named, named.max);
        break;
    case ValueKind::choice:
        range = option_list(named);
        break;
    }
    const std::string default_shown = named.kind == ValueKind::choice ?
                                          named.default_text :
                                          format_amount(named, named.default_value);
    return "# " + named.description + " " + range + ", default " + default_shown + ".\n";
}

/* The value as the file holds it, as the file writes it: under an
 * override, the file's own; `at_default`, the default. */
std::string written(const NamedValue& named, bool at_default);

std::string format_line(const NamedValue& named, bool at_default) {
    return named.key + " = " + written(named, at_default) + "\n";
}

/* Text compared ignoring ASCII case: "off" names the option "Off". */
bool same_text(std::string_view a, std::string_view b) {
    return a.size() == b.size() && lowered(a) == lowered(b);
}

/* The option whose text this is, ignoring case; -1 for none. */
int find_option(const ChoiceOptions& options, std::string_view text) {
    for (std::size_t i = 0; i < options.size(); ++i) {
        if (same_text(options[i].text, text)) {
            return static_cast<int>(i);
        }
    }
    return -1;
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

/* A value held to its range and, for a toggle, to 0 or 1. */
float held(const NamedValue& named, float value) {
    value = std::clamp(value, named.min, named.max);
    if (named.kind == ValueKind::toggle) {
        value = value != 0.0f ? 1.0f : 0.0f;
    }
    return value;
}

/* The value as the file holds it: under an override, the file's own. */
float file_value(const NamedValue& named) {
    return named.overridden ? named.saved : named.value;
}

std::string written(const NamedValue& named, bool at_default) {
    if (named.kind == ValueKind::choice) {
        return at_default ? named.default_text : named.text;
    }
    const float value = at_default ? named.default_value : file_value(named);
    return named.kind == ValueKind::toggle ? format_amount(named, value) : format_number(value);
}

}  // namespace

std::string format_value(const NamedValue& named) {
    switch (named.kind) {
    case ValueKind::toggle:
        return format_amount(named, named.value);
    case ValueKind::choice:
        return named.text;
    case ValueKind::number:
        break;
    }
    return format_number(named.value);
}

ChoiceOptions choice_options(const NamedValue& named) {
    return named.list ? named.list() : named.options;
}

int choice_index(const NamedValue& named, const ChoiceOptions& options) {
    if (options.empty()) {
        return -1;
    }
    int index = find_option(options, named.text);
    if (index < 0 && named.fallback) {
        index = find_option(options, named.fallback(named.text, options));
    }
    if (index < 0) {
        index = find_option(options, named.default_text);
    }
    return index < 0 ? 0 : index;
}

std::string choice_step(const NamedValue& named, const ChoiceOptions& options, int direction) {
    const int count = static_cast<int>(options.size());
    if (count == 0) {
        return {};
    }
    const int current = choice_index(named, options);
    const int index = ((current + direction) % count + count) % count;
    if (index == current) {
        /* One option: the saved text stays, even one the fallback matched. */
        return named.text;
    }
    return options[static_cast<std::size_t>(index)].text;
}

std::string Choice::option() const {
    const ChoiceOptions options = choice_options(*m_named);
    const int index = choice_index(*m_named, options);
    return index < 0 ? std::string() : options[static_cast<std::size_t>(index)].text;
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
    named.shown = spec.shown;
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
    named.shown = spec.shown;
    return Number(m_model->add(m_index, std::move(named)));
}

Choice ValueGroup::add_choice(const ChoiceSpec& spec) {
    NamedValue named;
    named.key = spec.name;
    named.label = spec.label;
    named.description = spec.description;
    named.kind = ValueKind::choice;
    named.default_value = 0.0f;
    named.min = 0.0f;
    named.max = 0.0f;
    named.step = 0.0f;
    named.default_text = spec.default_option;
    named.options = spec.options;
    named.list = spec.list;
    named.fallback = spec.fallback;
    named.none = spec.none;
    named.shown = spec.shown;
    return Choice(m_model->add(m_index, std::move(named)));
}

const NamedAction& ValueGroup::add_action(const ActionSpec& spec) {
    const ValueModel::Group& owner = m_model->m_groups[m_index];
    NamedAction& added = m_model->m_actions.emplace_back(
        NamedAction{owner.id + "." + spec.name, owner.id, spec.label, spec.description});
    m_model->m_shown.push_back(ShownRow{nullptr, &added});
    return added;
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
    named.text = named.default_text;
    /* A key registered twice is a programming error; the second gets the
     * first's value rather than a duplicate line in the file. */
    if (NamedValue* existing = find_mutable(named.key)) {
        return *existing;
    }
    NamedValue& added = m_values.emplace_back(std::move(named));
    owner.values.push_back(&added);
    if (added.shown) {
        m_shown.push_back(ShownRow{&added, nullptr});
    }
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

const ValueModel::Group* ValueModel::group_of(const NamedValue& named) const {
    for (const Group& group : m_groups) {
        if (std::find(group.values.begin(), group.values.end(), &named) != group.values.end()) {
            return &group;
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
    if (target == nullptr || target->kind == ValueKind::choice || !std::isfinite(value)) {
        return false;
    }
    value = held(*target, value);
    bool changed = value != target->value || value != file_value(*target);
    target->overridden = false;
    target->value = value;
    return changed;
}

bool ValueModel::choose(const NamedValue& named, std::string_view text) {
    NamedValue* target = find_mutable(named.key);
    if (target == nullptr || target->kind != ValueKind::choice || target->text == text) {
        return false;
    }
    target->text = std::string(text);
    return true;
}

bool ValueModel::override_value(const NamedValue& named, float value) {
    NamedValue* target = find_mutable(named.key);
    if (target == nullptr || target->kind == ValueKind::choice || !std::isfinite(value)) {
        return false;
    }
    if (!target->overridden) {
        target->saved = target->value;
        target->overridden = true;
    }
    value = held(*target, value);
    bool changed = value != target->value;
    target->value = value;
    return changed;
}

std::string ValueModel::to_text(bool at_defaults) const {
    std::vector<const NamedValue*> all;
    for (const NamedValue& named : m_values) {
        all.push_back(&named);
    }
    std::string text = write(all, at_defaults);
    if (!m_unknown.empty()) {
        text += "\n# " + std::string(kUnknownTitle) + "\n\n";
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
            /* Under an override, the file's own value: an override is never written. */
            text += "\n" + describe(*named) + format_line(*named, at_defaults);
        }
    }
    return text;
}

std::string ValueModel::edit_text(std::string_view text, const NamedValue& named) const {
    const Group* owner = group_of(named);
    if (owner == nullptr) {
        return std::string(text);
    }
    /* The text cut into lines, each with its own ending, so the ones left
     * alone go back byte for byte. */
    std::vector<std::string_view> lines;
    for (std::string_view rest = text; !rest.empty();) {
        std::size_t end = rest.find('\n');
        std::size_t length = end == std::string_view::npos ? rest.size() : end + 1;
        lines.push_back(rest.substr(0, length));
        rest.remove_prefix(length);
    }
    const std::string value = written(named, false);
    std::size_t found = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::string_view body = line_body(lines[i]);
        body = body.substr(0, body.find('#'));
        std::size_t equals = body.find('=');
        if (equals != std::string_view::npos && trim(body.substr(0, equals)) == named.key) {
            found = i;
        }
    }
    std::string out;
    if (found < lines.size()) {
        /* Only the value is replaced: the key as written, the spaces around
         * the equals sign, a comment after the value and the line's ending
         * stay. */
        std::string_view line = lines[found];
        std::string_view body = line_body(line);
        std::size_t start = body.find('=') + 1;
        while (start < body.size() && (body[start] == ' ' || body[start] == '\t')) {
            ++start;
        }
        std::size_t end = std::min(body.find('#'), body.size());
        while (end > start && (body[end - 1] == ' ' || body[end - 1] == '\t')) {
            --end;
        }
        /* A line with no value gets spaces of its own. */
        const bool empty = start == end;
        std::string edited(line.substr(0, start));
        if (empty && edited.back() == '=') {
            edited += ' ';
        }
        edited += value;
        if (empty && end < body.size()) {
            edited += ' ';
        }
        edited += line.substr(end);
        for (std::size_t i = 0; i < lines.size(); ++i) {
            out += i == found ? std::string_view(edited) : lines[i];
        }
        return out;
    }
    /* New lines end as the text's first line does: Notepad may save
     * \r\n. */
    const bool crlf = !lines.empty() && lines[0].ends_with("\r\n");
    auto with_eol = [crlf](std::string_view block) {
        std::string converted;
        for (char c : block) {
            converted += crlf && c == '\n' ? std::string_view("\r\n") : std::string_view(&c, 1);
        }
        return converted;
    };
    auto heading_title = [](std::string_view line) {
        std::string_view body = trim(line_body(line));
        return body.starts_with('#') ? trim(body.substr(1)) : std::string_view();
    };
    std::size_t heading = 0;
    while (heading < lines.size() && heading_title(lines[heading]) != owner->title) {
        ++heading;
    }
    std::string entry = "\n" + describe(named) + format_line(named, false);
    if (heading == lines.size()) {
        out = std::string(text);
        if (!out.empty() && out.back() != '\n') {
            out += with_eol("\n");
        }
        return out + with_eol("\n# " + owner->title + "\n" + entry);
    }
    /* The group's end: the next heading of another group, or the end. */
    std::size_t next = heading + 1;
    for (; next < lines.size(); ++next) {
        std::string_view title = heading_title(lines[next]);
        bool group_heading = !title.empty() && title == kUnknownTitle;
        for (const Group& group : m_groups) {
            group_heading = group_heading || (!title.empty() && &group != owner &&
                                                 title == std::string_view(group.title));
        }
        if (group_heading) {
            break;
        }
    }
    /* After the group's last line that is not blank. */
    std::size_t after = next;
    while (after > heading + 1 && trim(line_body(lines[after - 1])).empty()) {
        --after;
    }
    for (std::size_t i = 0; i < after; ++i) {
        out += lines[i];
    }
    if (!out.empty() && out.back() != '\n') {
        out += with_eol("\n");
    }
    out += with_eol(entry);
    for (std::size_t i = after; i < lines.size(); ++i) {
        out += lines[i];
    }
    return out;
}

TextRead ValueModel::read_text(std::string_view text) {
    TextRead read;
    /* The value each known key's line gives, or the value it held when the
     * line cannot be read. */
    std::map<NamedValue*, float> given;
    /* The same for choices: the text each gives. */
    std::map<NamedValue*, std::string> given_text;
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
        if (named->kind == ValueKind::choice) {
            if (value.empty()) {
                read.bad_lines.push_back(number);
                read.notes.push_back("line " + std::to_string(number) +
                                     " not understood: " + std::string(trim(raw)) + "; " +
                                     named->key + " stays " + named->text);
                given_text.try_emplace(named, named->text);
                continue;
            }
            /* Only fixed options are checked: a run-time list may not be
             * known yet, and its owner says what it falls back to. */
            if (!named->list && !named->options.empty() && find_option(named->options, value) < 0) {
                NamedValue probe = *named;
                probe.text = std::string(value);
                const int index = choice_index(probe, named->options);
                read.notes.push_back("line " + std::to_string(number) + ": " + named->key + " = " +
                                     std::string(value) + " is none of the options; using " +
                                     named->options[static_cast<std::size_t>(index)].text);
            }
            given_text[named] = std::string(value);
            continue;
        }
        float parsed = 0.0f;
        bool ok = named->kind == ValueKind::toggle ? read_toggle(value, parsed) :
                                                     read_number(*named, value, parsed);
        if (!ok) {
            read.bad_lines.push_back(number);
            read.notes.push_back("line " + std::to_string(number) +
                                 " not understood: " + std::string(trim(raw)) + "; " + named->key +
                                 " stays " + format_amount(*named, file_value(*named)));
            given.try_emplace(named, file_value(*named));
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
        if (named.kind == ValueKind::choice) {
            auto found = given_text.find(&named);
            std::string text = named.default_text;
            if (found != given_text.end()) {
                text = found->second;
            } else {
                read.missing.push_back(&named);
            }
            if (text != named.text) {
                named.text = std::move(text);
                read.changed.push_back(&named);
            }
            continue;
        }
        auto found = given.find(&named);
        float value = named.default_value;
        if (found != given.end()) {
            value = found->second;
        } else {
            read.missing.push_back(&named);
        }
        if (named.overridden) {
            /* The file's value waits under the override, unannounced. */
            if (value != named.saved) {
                read.notes.push_back(named.key + " = " + format_amount(named, value) +
                                     " in the file, held at " + format_amount(named, named.value) +
                                     " for this run");
            }
            named.saved = value;
            continue;
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
