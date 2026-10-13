/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The fork's settings model (primer docs/a11y/cues.md, "Cue settings"):
 * named values in groups, each with a key, a default, a range, a unit and a
 * one-line description, enough for a front end to draw a switch or a slider
 * for it and for the settings file to explain itself. Features register
 * their own values and read them through typed handles; the model knows
 * nothing about any feature.
 *
 * The text format is part of the model, so every front end writing a file
 * writes the same one: `key = value` a line, grouped under `#` headings,
 * each value preceded by its description. settings_file.hpp reads and writes
 * it on disc. Game thread only. */
#pragma once
#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

enum class SettingKind {
    toggle, /* on or off: a switch */
    number, /* a value in a range: a slider */
};

/* One value. A toggle holds 0 or 1, with the range 0 to 1 and step 1. */
struct Setting {
    std::string key;         /* "cues.volume": the group's id, a dot, the name */
    std::string label;       /* a menu row's name: "Cue volume" */
    std::string description; /* one line: the file's comment, a menu's help */
    std::string unit;        /* "%", "s", "octaves", "units", "times" or "" */
    SettingKind kind = SettingKind::number;
    float default_value = 0.0f;
    float min = 0.0f;
    float max = 1.0f;
    float step = 1.0f; /* a slider's step; the file takes any value in range */
    float value = 0.0f;
};

struct SwitchSpec {
    std::string name; /* the key's last part: "on" */
    std::string label;
    std::string description;
    bool default_on = true;
};

struct NumberSpec {
    std::string name; /* the key's last part: "volume" */
    std::string label;
    std::string description;
    std::string unit;
    float default_value = 0.0f;
    float min = 0.0f;
    float max = 1.0f;
    float step = 1.0f;
};

/* Typed handles: read a value without looking up its key. Valid for the
 * model's life. */
class Switch {
public:
    explicit Switch(const Setting& setting) : m_setting(&setting) {}
    bool on() const { return m_setting->value != 0.0f; }
    const Setting& setting() const { return *m_setting; }

private:
    const Setting* m_setting;
};

class Number {
public:
    explicit Number(const Setting& setting) : m_setting(&setting) {}
    float value() const { return m_setting->value; }
    const Setting& setting() const { return *m_setting; }

private:
    const Setting* m_setting;
};

class Settings;

/* Registers values under one group: its id prefixes their keys, its title
 * heads them in the file and, later, in the port menu. */
class SettingsGroup {
public:
    SettingsGroup(Settings& settings, std::size_t index) : m_settings(&settings), m_index(index) {}
    Switch add_switch(const SwitchSpec& spec);
    Number add_number(const NumberSpec& spec);

private:
    Settings* m_settings;
    std::size_t m_index;
};

/* What reading a text did. */
struct SettingsRead {
    /* Values whose value the text changed, a value the text lacks going back
     * to its default included. */
    std::vector<const Setting*> changed;
    /* Known values the text lacks, now at their defaults. */
    std::vector<const Setting*> missing;
    /* Lines, counted from 1, that could not be read: each keeps its value's
     * last setting. */
    std::vector<int> bad_lines;
    /* Keys the text has and the model does not. */
    std::vector<std::string> unknown;
    /* Log lines about the text: what was not understood, what was clamped. */
    std::vector<std::string> notes;
};

/* A value as the file writes it: "on", "off" or a number. */
std::string format_value(const Setting& setting);

class Settings {
public:
    struct Group {
        std::string id;
        std::string title;
        std::vector<Setting*> settings;
    };

    Settings() = default;
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    /* A new group, or the one with this id. */
    SettingsGroup group(std::string_view id, std::string_view title);
    const std::vector<Group>& groups() const { return m_groups; }
    const Setting* find(std::string_view key) const;

    /* A front end's change: held to the range. True when the value
     * changed. */
    bool set(const Setting& setting, float value);

    /* The whole model as the file's text, every value at its current value
     * or its default, the keys kept from an earlier read last. */
    std::string to_text(bool at_defaults = false) const;
    /* Only these values, at their current values, under their groups'
     * headings: what to append to a file that lacks them. */
    std::string to_text(const std::vector<const Setting*>& only) const;
    /* Applies a text to the model: values it lacks go back to their
     * defaults, a line that cannot be read keeps its value's last setting,
     * a value out of range is clamped. */
    SettingsRead read_text(std::string_view text);

    /* Keys a text had that the model does not, with their values as
     * written, kept so a front end that rewrites the file keeps them too. */
    const std::map<std::string, std::string>& unknown() const { return m_unknown; }

private:
    friend class SettingsGroup;
    Setting& add(std::size_t group, Setting setting);
    Setting* find_mutable(std::string_view key);
    std::string write(const std::vector<const Setting*>& only, bool at_defaults) const;

    std::deque<Setting> m_settings; /* a deque, so handles stay valid */
    std::vector<Group> m_groups;
    std::map<std::string, std::string> m_unknown;
};

}  // namespace a11y
