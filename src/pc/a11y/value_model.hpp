/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The fork's model of named values, which holds the accessibility settings
 * (primer docs/a11y/cues.md, "Accessibility settings"): named values in
 * groups, each with a key, a default, a range or a list of options, a unit
 * and a one-line description, enough for a front end to draw a switch, a
 * slider or a choice for it and for the model's file to explain itself. A
 * value registered as shown is a setting, a row of the menus; the rest are
 * tuning values, in the file alone. Features register their own values and
 * read them through typed handles; the model knows nothing about any
 * feature.
 *
 * The text format is part of the model, so every front end writing a file
 * writes the same one: `key = value` a line, grouped under `#` headings,
 * each value preceded by its description. value_file.hpp reads and writes
 * it on disc. Game thread only. */
#pragma once
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

enum class ValueKind {
    toggle, /* on or off: a switch */
    number, /* a value in a range: a slider */
    choice, /* one of a list of named options: a row Left and Right step through */
};

/* One option of a choice. */
struct ChoiceOption {
    /* As the file saves it: "Microsoft Mark". Spaces at either end and a #,
     * which starts a comment there, would not read back. */
    std::string text;
    /* As the row shows it, empty for the text: "Microsoft Mark, English
     * (United States)". */
    std::string label;
    const std::string& shown() const { return label.empty() ? text : label; }
};
using ChoiceOptions = std::vector<ChoiceOption>;
/* A choice's options asked at run time, each time they are needed: every
 * frame of an open menu page among others, so it must be cheap (a copy of a
 * list kept elsewhere). Game thread. */
using ChoiceList = std::function<ChoiceOptions()>;
/* The option in use when the saved text names none of the options listed
 * now: the text of one of them. The saved text and the options are its
 * arguments, so it can match by part of a name ("Zira" in "Microsoft
 * Zira"). */
using ChoiceFallback =
    std::function<std::string(std::string_view saved, const ChoiceOptions& options)>;

/* One value. A toggle holds 0 or 1, with the range 0 to 1 and step 1. A
 * choice holds the text of an option (text) and leaves the numbers at 0. */
struct NamedValue {
    std::string key;         /* "cues.volume": the group's id, a dot, the name */
    std::string label;       /* a menu row's name: "Cue volume" */
    std::string description; /* one line: the file's comment, a menu's help */
    std::string unit;        /* "%", "s", "octaves", "units", "times" or "" */
    ValueKind kind = ValueKind::number;
    float default_value = 0.0f;
    float min = 0.0f;
    float max = 1.0f;
    float step = 1.0f;  /* a slider's step; the file takes any value in range */
    float value = 0.0f; /* what the game uses: an override while one holds */
    bool shown = false; /* a setting, a row of the menus; else a tuning value */
    /* An override from the environment holds value for the run
     * (ValueModel::override_value); the file's own value waits in saved, read
     * and written as if no override held. */
    bool overridden = false;
    float saved = 0.0f;
    /* A choice: the option's text as the file holds it, which may name no
     * option listed now (a voice uninstalled), kept as it is until the
     * player picks another; its default; and its options, fixed at
     * registration or asked of list at run time, with the fallback for a
     * text that names none (choice_index). A choice has no override. */
    std::string text;
    std::string default_text;
    ChoiceOptions options;
    ChoiceList list;
    ChoiceFallback fallback;
};

/* A row of the menus that acts rather than holds a value: a button. Never
 * in the file. */
struct NamedAction {
    std::string key;   /* "gap_warning.preview": the group's id, a dot, the name */
    std::string group; /* the group's id: for a cue's Preview, the cue's id */
    std::string label; /* "Preview gap warning" */
    std::string description;
};

/* One row of the menus, in registration order: a value or an action. */
struct ShownRow {
    const NamedValue* value = nullptr;   /* a switch or a slider, by its kind */
    const NamedAction* action = nullptr; /* a button */
};

struct SwitchSpec {
    std::string name; /* the key's last part: "on" */
    std::string label;
    std::string description;
    bool default_on = true;
    bool shown = false;
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
    bool shown = false;
};

/* Options either fixed (`options`) or asked at run time (`list`, which then
 * wins). With no fallback, a saved text that names no option gives the
 * default's option, or the first. Designated initializers read best:
 * ChoiceSpec{.name = "voice", .label = "Your voice", ..., .list = voices}. */
struct ChoiceSpec {
    std::string name; /* the key's last part: "voice" */
    std::string label;
    std::string description;
    std::string default_option; /* an option's text, or a text the fallback understands */
    ChoiceOptions options;
    ChoiceList list;
    ChoiceFallback fallback;
    bool shown = false;
};

struct ActionSpec {
    std::string name; /* the key's last part: "preview" */
    std::string label;
    std::string description;
};

/* Typed handles: read a value without looking up its key. Valid for the
 * model's life. */
class Switch {
public:
    explicit Switch(const NamedValue& named) : m_named(&named) {}
    bool on() const { return m_named->value != 0.0f; }
    const NamedValue& named() const { return *m_named; }

private:
    const NamedValue* m_named;
};

class Number {
public:
    explicit Number(const NamedValue& named) : m_named(&named) {}
    float value() const { return m_named->value; }
    const NamedValue& named() const { return *m_named; }

private:
    const NamedValue* m_named;
};

/* A choice's options as they stand: the registration's, or what its list
 * gives now. */
ChoiceOptions choice_options(const NamedValue& named);
/* The option in use among these options: the one whose text is the saved
 * text, ignoring ASCII case; else the one the fallback names; else the
 * default's; else the first. -1 with no options. */
int choice_index(const NamedValue& named, const ChoiceOptions& options);
/* The text of the option a step Left (-1) or Right (1) from the one in use
 * lands on, wrapping round at either end as the base port's own choice rows
 * do; empty with no options. */
std::string choice_step(const NamedValue& named, const ChoiceOptions& options, int direction);

class Choice {
public:
    explicit Choice(const NamedValue& named) : m_named(&named) {}
    /* The text of the option in use (choice_index); empty with no options.
     * Asks a run-time list each time. */
    std::string option() const;
    ChoiceOptions options() const { return choice_options(*m_named); }
    /* The text the file holds, whether or not it names an option now. */
    const std::string& saved() const { return m_named->text; }
    const NamedValue& named() const { return *m_named; }

private:
    const NamedValue* m_named;
};

class ValueModel;

/* Registers values under one group: its id prefixes their keys, its title
 * heads them in the file. The menus have no headings: each shown row's label
 * names what it is about. */
class ValueGroup {
public:
    ValueGroup(ValueModel& model, std::size_t index) : m_model(&model), m_index(index) {}
    Switch add_switch(const SwitchSpec& spec);
    Number add_number(const NumberSpec& spec);
    Choice add_choice(const ChoiceSpec& spec);
    /* A button, always shown. */
    const NamedAction& add_action(const ActionSpec& spec);

private:
    ValueModel* m_model;
    std::size_t m_index;
};

/* What reading a text did. */
struct TextRead {
    /* Values whose value the text changed, a value the text lacks going back
     * to its default included. */
    std::vector<const NamedValue*> changed;
    /* Known values the text lacks, now at their defaults. */
    std::vector<const NamedValue*> missing;
    /* Lines, counted from 1, that could not be read: the value on each keeps
     * what it held. */
    std::vector<int> bad_lines;
    /* Keys the text has and the model does not. */
    std::vector<std::string> unknown;
    /* Log lines about the text: what was not understood, what was clamped. */
    std::vector<std::string> notes;
};

/* A value as the file writes it: "on", "off", a number or a choice's text. */
std::string format_value(const NamedValue& named);

class ValueModel {
public:
    struct Group {
        std::string id;
        std::string title;
        std::vector<NamedValue*> values;
    };

    ValueModel() = default;
    ValueModel(const ValueModel&) = delete;
    ValueModel& operator=(const ValueModel&) = delete;

    /* A new group, or the one with this id. */
    ValueGroup group(std::string_view id, std::string_view title);
    const std::vector<Group>& groups() const { return m_groups; }
    const NamedValue* find(std::string_view key) const;
    /* The menus' rows: the shown values and the actions, in the order they
     * were registered, which is the file's order too. A front end walks
     * this list and needs none of its own. */
    const std::vector<ShownRow>& shown() const { return m_shown; }

    /* A front end's change: held to the range. True when the value
     * changed. A change to an overridden value lifts the override, so the
     * value follows the file again from then on; it counts as a change when
     * the file's value differs too, so the change is saved. */
    bool set(const NamedValue& named, float value);
    /* A front end's choice: the option's text, saved as it is. True when
     * the saved text changed. False for a value that is no choice. */
    bool choose(const NamedValue& named, std::string_view text);
    /* Holds the value at value for the run, held to the range, whatever the
     * file says: an environment variable's override. The file keeps its own
     * value, which reading a text updates silently and writing the text
     * writes. True when the value the game uses changed. A choice has no
     * override: false, and nothing held. */
    bool override_value(const NamedValue& named, float value);

    /* The whole model as the file's text, every value at the file's value
     * (under an override, the file's own) or its default, the keys kept
     * from an earlier read last. */
    std::string to_text(bool at_defaults = false) const;
    /* Only these values, at the file's values, under their groups'
     * headings: what to append to a file that lacks them. */
    std::string to_text(const std::vector<const NamedValue*>& only) const;
    /* Applies a text to the model: values it lacks go back to their
     * defaults, a line that cannot be read leaves its value as it was,
     * a value out of range is clamped. */
    TextRead read_text(std::string_view text);
    /* A text with one value changed, as the file is saved after a front
     * end's change: the value on the key's last line (the one read_text
     * takes) replaced by its current value, or, with no such line, the line
     * and its description added at the end of its group, under the group's
     * heading, or at the text's end with its heading when that is missing
     * too. Everything else stays as written. */
    std::string edit_text(std::string_view text, const NamedValue& named) const;

    /* Keys a text had that the model does not, with their values as
     * written, kept so a front end that rewrites the file keeps them too. */
    const std::map<std::string, std::string>& unknown() const { return m_unknown; }

private:
    friend class ValueGroup;
    NamedValue& add(std::size_t group, NamedValue named);
    NamedValue* find_mutable(std::string_view key);
    const Group* group_of(const NamedValue& named) const;
    std::string write(const std::vector<const NamedValue*>& only, bool at_defaults) const;

    std::deque<NamedValue> m_values; /* a deque, so handles stay valid */
    std::deque<NamedAction> m_actions;
    std::vector<Group> m_groups;
    std::vector<ShownRow> m_shown;
    std::map<std::string, std::string> m_unknown;
};

}  // namespace a11y
