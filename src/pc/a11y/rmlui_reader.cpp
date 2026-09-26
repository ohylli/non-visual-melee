/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rmlui_reader.hpp"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>

namespace a11y {
namespace {

void collect_text(const Rml::Element& element, std::string& out) {
    if (const auto* text = dynamic_cast<const Rml::ElementText*>(&element)) {
        out += text->GetText();
        out += ' ';
        return;
    }
    /* A line break reads as a pause, not as two words run together. */
    if (element.GetTagName() == "br") {
        out += ", ";
        return;
    }
    for (int i = 0; i < element.GetNumChildren(); ++i) {
        const Rml::Element* child = element.GetChild(i);
        if (child->IsVisible()) {
            collect_text(*child, out);
        }
    }
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string collapse_whitespace(std::string_view text) {
    std::string out;
    bool space = false;
    for (const char c : text) {
        if (is_space(c)) {
            space = !out.empty();
            continue;
        }
        /* No space before a comma a line break added. */
        if (space && c != ',') {
            out += ' ';
        }
        space = false;
        out += c;
    }
    return out;
}

/* Another element of the same document, by id. */
const Rml::Element* find_in_document(const Rml::Element& element, const Rml::String& id) {
    Rml::ElementDocument* document = element.GetOwnerDocument();
    return document != nullptr ? document->GetElementById(id) : nullptr;
}

/* A setting row: a description (heading and help sentence) beside a control. */
const Rml::Element* row_of(const Rml::Element& element) {
    for (const Rml::Element* e = element.GetParentNode(); e != nullptr; e = e->GetParentNode()) {
        if (e->IsClassSet("setting")) {
            return e;
        }
    }
    return nullptr;
}

/* The text of the first descendant with the tag, empty if there is none. */
std::string first_text(const Rml::Element& element, const char* tag) {
    Rml::ElementList found;
    const_cast<Rml::Element&>(element).GetElementsByTagName(found, tag);
    return found.empty() ? std::string() : element_text(*found.front());
}

bool has_control(const Rml::Element& element) {
    if (is_control(element)) {
        return true;
    }
    for (int i = 0; i < element.GetNumChildren(); ++i) {
        if (has_control(*element.GetChild(i))) {
            return true;
        }
    }
    return false;
}

/* Headings and paragraphs, each as its own sentence: "Keyboard. WASD...". */
void collect_sentences(const Rml::Element& element, std::string& out) {
    const Rml::String& tag = element.GetTagName();
    if (tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "p") {
        append_sentence(out, element_text(element));
        return;
    }
    for (int i = 0; i < element.GetNumChildren(); ++i) {
        collect_sentences(*element.GetChild(i), out);
    }
}

/* GetValue is not const in RmlUi, though it only reads. */
std::string form_value(const Rml::Element& element) {
    auto* form = dynamic_cast<Rml::ElementFormControl*>(const_cast<Rml::Element*>(&element));
    return form != nullptr ? collapse_whitespace(form->GetValue()) : std::string();
}

void describe_tab(const Rml::Element& tab, Control& control) {
    control.role = Role::tab;
    TabPlace& place = control.tab;
    place.selected = tab.IsClassSet("selected");
    const Rml::Element* strip = tab.GetParentNode();
    for (int i = 0; strip != nullptr && i < strip->GetNumChildren(); ++i) {
        const Rml::Element* sibling = strip->GetChild(i);
        if (sibling->IsClassSet("tab")) {
            ++place.count;
            if (sibling == &tab) {
                place.position = place.count;
            }
        }
    }
}

}  // namespace

std::string element_text(const Rml::Element& element) {
    std::string text;
    collect_text(element, text);
    return collapse_whitespace(text);
}

bool is_control(const Rml::Element& element) {
    const Rml::String& tag = element.GetTagName();
    return tag == "button" || tag == "input" || tag == "select" || tag == "textarea";
}

Control describe_control(const Rml::Element& element) {
    Control control;
    control.disabled = element.IsPseudoClassSet("disabled");
    if (const Rml::Element* row = row_of(element)) {
        control.name = first_text(*row, "h3");
        control.help = first_text(*row, "p");
    }
    const Rml::String& tag = element.GetTagName();
    if (tag == "button") {
        control.value = element_text(element);
        if (element.IsClassSet("tab")) {
            describe_tab(element, control);
            control.name = control.value;
        } else if (!control.name.empty()) {
            control.role = Role::setting;
        } else {
            control.role = Role::button;
            control.name = control.value;
        }
        return control;
    }
    control.value = form_value(element);
    if (tag == "input") {
        const Rml::String type = element.GetAttribute<Rml::String>("type", "text");
        if (type == "range") {
            control.role = Role::slider;
            /* The span beside a slider shows its value as the player reads it
             * ("80%", "Auto"); the slider's own value is a bare number. */
            if (const Rml::Element* shown = find_in_document(element, element.GetId() + "-val")) {
                control.value = element_text(*shown);
            }
        } else if (type == "text" || type == "password") {
            control.role = Role::edit;
        }
    }
    return control;
}

void make_action(Control& control) {
    control.role = Role::button;
    control.name = control.name.empty() ? control.value : control.name + ": " + control.value;
}

std::string field_value(const Control& control) {
    return control.value.empty() ? "blank" : control.value;
}

std::string focus_announcement(const Control& control) {
    std::string text = control.name;
    const auto add = [&text](std::string_view part) {
        if (part.empty()) {
            return;
        }
        if (!text.empty()) {
            text += ", ";
        }
        text += part;
    };
    std::string_view hint;
    switch (control.role) {
    case Role::button:
        add("button");
        break;
    case Role::setting:
        add(control.value);
        break;
    case Role::slider:
        add("slider");
        add(control.value);
        hint = "Left and right to adjust";
        break;
    case Role::edit:
        add("edit");
        add(field_value(control));
        break;
    case Role::tab:
        add("tab");
        if (control.tab.selected) {
            add("selected");
        }
        add(std::to_string(control.tab.position) + " of " + std::to_string(control.tab.count));
        hint = "Left and right to switch tabs";
        break;
    case Role::other:
        if (control.value != control.name) {
            add(control.value);
        }
        break;
    }
    if (control.disabled) {
        add("unavailable");
    } else if (control.role == Role::setting) {
        add("Enter to change");
    } else {
        append_sentence(text, hint);
    }
    append_sentence(text, control.help);
    return text;
}

std::string tab_page_text(const Rml::Element& tab) {
    const Rml::String& id = tab.GetId();
    if (id.rfind("tab-", 0) != 0) {
        return "";
    }
    const Rml::Element* page = find_in_document(tab, "page-" + id.substr(4));
    std::string text;
    for (int i = 0; page != nullptr && i < page->GetNumChildren(); ++i) {
        const Rml::Element* row = page->GetChild(i);
        if (!has_control(*row)) {
            collect_sentences(*row, text);
        }
    }
    return text;
}

std::string clean_text(std::string_view text) {
    static constexpr std::string_view kSeparators[] = {
        " / ", " \xC2\xB7 ", " \xE2\x86\x92 "}; /* "/", "·", "→" */
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        bool replaced = false;
        for (const std::string_view separator : kSeparators) {
            if (text.substr(i, separator.size()) == separator) {
                out += ", ";
                i += separator.size();
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            out += text[i++];
        }
    }
    return out;
}

void append_sentence(std::string& announcement, std::string_view sentence) {
    if (sentence.empty()) {
        return;
    }
    if (!announcement.empty()) {
        const char last = announcement.back();
        announcement += (last == '.' || last == '!' || last == '?') ? " " : ". ";
    }
    announcement += sentence;
}

}  // namespace a11y
