/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rmlui_reader.hpp"
#include <RmlUi/Core/Element.h>
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
        if (space) {
            out += ' ';
            space = false;
        }
        out += c;
    }
    return out;
}

std::string role_of(const Rml::Element& element) {
    const Rml::String& tag = element.GetTagName();
    if (tag == "button") {
        return "button";
    }
    if (tag == "input") {
        const Rml::String type = element.GetAttribute<Rml::String>("type", "text");
        if (type == "range") {
            return "slider";
        }
        if (type == "text" || type == "password") {
            return "edit";
        }
    }
    return "";
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
    control.role = role_of(element);
    control.disabled = element.IsPseudoClassSet("disabled");
    /* GetValue is not const in RmlUi, though it only reads. */
    if (auto* form = dynamic_cast<Rml::ElementFormControl*>(const_cast<Rml::Element*>(&element))) {
        control.value = collapse_whitespace(form->GetValue());
    } else {
        control.value = element_text(element);
        control.name = control.value;
    }
    return control;
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
    add(control.role);
    if (control.value != control.name) {
        add(control.value);
    }
    if (control.disabled) {
        add("unavailable");
    }
    return text;
}

std::string clean_text(std::string_view text) {
    static constexpr std::string_view kSeparators[] = {" / ", " \xC2\xB7 "}; /* "/", "·" */
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
