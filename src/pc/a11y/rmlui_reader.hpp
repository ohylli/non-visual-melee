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

/* A focusable control as the player hears it. */
struct Control {
    std::string name;  /* "Choose disc" */
    std::string role;  /* "button", "slider", "edit"; empty when unknown */
    std::string value; /* what a value change speaks: a button's label, a field's value */
    bool disabled = false;
};

/* The visible text inside an element, whitespace collapsed and trimmed. */
std::string element_text(const Rml::Element& element);

/* An element the player can act on: a button or a form control. Focus parked
 * anywhere else (a container, the document) is not announced. */
bool is_control(const Rml::Element& element);

Control describe_control(const Rml::Element& element);

/* "Choose disc, button", or "Choose disc, button, unavailable". */
std::string focus_announcement(const Control& control);

/* Turns the page's visual separators into commas: "Ready to play / Disc not
 * verified." reads "Ready to play, Disc not verified.". Applied to a whole
 * announcement just before it is spoken. */
std::string clean_text(std::string_view text);

/* Appends a sentence to an announcement, adding ". " unless the text so far
 * already ends a sentence. */
void append_sentence(std::string& announcement, std::string_view sentence);

}  // namespace a11y
