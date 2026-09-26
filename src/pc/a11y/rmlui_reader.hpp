/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Generic reading of an RmlUi page: what a control is called, what it is and
 * what value it shows, all taken from the page's own text. Knows nothing about
 * any one page; the launcher's specifics live in launcher_speech.cpp, and the
 * port menu is meant to reuse this. docs/a11y/rmlui-screens.md explains it. */
#pragma once
#include <string>
#include <string_view>

namespace Rml {
class Element;
}

namespace a11y {

enum class Role {
    button,  /* acts when pressed: "Choose disc" */
    setting, /* a button in a setting row whose label is the value; Enter cycles it */
    slider,
    edit,
    tab,
    other,
};

/* A tab's place in its strip. */
struct TabPlace {
    bool selected = false;
    int position = 0; /* 1-based */
    int count = 0;
};

/* A focusable control as the player hears it. */
struct Control {
    Role role = Role::other;
    std::string name;  /* "Choose disc", "Vertical sync" */
    std::string value; /* what a value change speaks: a label, a slider's shown value */
    std::string help;  /* the setting row's help sentence; empty outside a row */
    bool disabled = false;
    TabPlace tab; /* tabs only */
};

/* The visible text inside an element, whitespace collapsed and trimmed. */
std::string element_text(const Rml::Element& element);

/* An element the player can act on: a button or a form control. Focus parked
 * anywhere else (a container, the document) is not announced. */
bool is_control(const Rml::Element& element);

Control describe_control(const Rml::Element& element);

/* A button in a setting row that acts rather than cycles ("Performance
 * preset", "Apply"). The page does not mark them, so the caller's override
 * table does: its name becomes "Performance preset: Apply". */
void make_action(Control& control);

/* A text field's value as spoken: "blank" when it is empty. */
std::string field_value(const Control& control);

/* "Choose disc, button", "Vertical sync, On, Enter to change. <help>",
 * "Master volume, slider, 80%. Left and right to adjust. <help>". */
std::string focus_announcement(const Control& control);

/* The text of every row on a tab's page that has no control, which focus
 * never reaches: "Your connect code. ABCD#123". The page is found by the
 * id convention the RmlUi pages share, "tab-x" to "page-x". */
std::string tab_page_text(const Rml::Element& tab);

/* Turns the page's visual separators into commas: "Ready to play / Disc not
 * verified." reads "Ready to play, Disc not verified.". Applied to a whole
 * announcement just before it is spoken. */
std::string clean_text(std::string_view text);

/* Appends a sentence to an announcement, adding ". " unless the text so far
 * already ends a sentence. */
void append_sentence(std::string& announcement, std::string_view sentence);

}  // namespace a11y
