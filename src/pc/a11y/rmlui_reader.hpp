/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Generic reading of an RmlUi page: what a control is called, what it is and
 * what value it shows, all taken from the page's own text. Knows nothing about
 * any one page; the launcher's specifics live in launcher_speech.cpp, and the
 * port menu is meant to reuse this. docs/a11y/rmlui-screens.md explains it. */
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
}  // namespace Rml

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

/* One stop on a tab's page in the order Up and Down move through it: a
 * control, or one line of a row that has none, which focus never reaches. */
struct PageStop {
    const Rml::Element* control = nullptr; /* null for a line */
    std::string line;
};

/* The selected tab of the document's tab strip (class "tab" and "selected"),
 * or null when it has none. */
const Rml::Element* selected_tab(const Rml::ElementDocument& document);

/* The tab followed by its page's stops in document order: a row with a
 * control gives its first control, a row without one its lines. A line is a
 * heading, or a piece of a paragraph between line breaks and " · "
 * separators: "Keyboard", "WASD / arrows Move", "X Attack". The page is found
 * by the id convention the RmlUi pages share, "tab-x" to "page-x"; a tab
 * without one gives the tab alone. */
std::vector<PageStop> tab_page_stops(const Rml::Element& tab);

/* A reading position on a line of a page, kept beside focus, which stays on
 * the control next to the lines. Up and Down go to it before the page's own
 * focus order: they move over the lines, and off the last one in either
 * direction they either hand back to the page, which moves focus on to the
 * control past the lines, or return to the control focus is still on. The
 * page's focus order must follow its stops for the hand-back to land there. */
class PageLines {
public:
    enum class Move {
        page,  /* not taken: the page moves focus as usual */
        line,  /* now on a line: speak line() */
        focus, /* back on the focused control: speak its focus announcement */
    };

    /* Up (-1) or Down (1) from the focused control, over the stops it is
     * one of. */
    Move step(const std::vector<PageStop>& stops, const Rml::Element& focus, int direction);
    const std::string& line() const { return m_line; }
    bool reading() const { return m_index >= 0; }
    void clear() { m_index = -1; }

private:
    int m_index = -1; /* the stop of the line being read, -1 when on focus */
    std::string m_line;
};

/* Turns the page's visual separators into commas: "Ready to play / Disc not
 * verified." reads "Ready to play, Disc not verified.". Applied to a whole
 * announcement just before it is spoken. */
std::string clean_text(std::string_view text);

/* Appends a sentence to an announcement, adding ". " unless the text so far
 * already ends a sentence. */
void append_sentence(std::string& announcement, std::string_view sentence);

}  // namespace a11y
