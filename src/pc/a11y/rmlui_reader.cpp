/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rmlui_reader.hpp"
#include "speech.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>
#include <algorithm>
#include <charconv>
#include <utility>

namespace a11y {
namespace {

/* What an empty field, or the end of its text, reads as. */
constexpr const char* kBlank = "blank";

std::string spoken_value(const std::string& value) {
    return value.empty() ? kBlank : value;
}

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

/* The first control inside the element, in document order. */
const Rml::Element* first_control(const Rml::Element& element) {
    if (is_control(element)) {
        return &element;
    }
    for (int i = 0; i < element.GetNumChildren(); ++i) {
        if (const Rml::Element* found = first_control(*element.GetChild(i))) {
            return found;
        }
    }
    return nullptr;
}

/* Ends the line gathered so far, split at " · " into its pieces. */
void end_line(std::string& piece, std::vector<std::string>& lines) {
    static constexpr std::string_view kDot = " \xC2\xB7 "; /* " · " */
    const std::string text = collapse_whitespace(piece);
    piece.clear();
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(kDot, start);
        if (end == std::string::npos) {
            end = text.size();
        }
        if (end > start) {
            lines.push_back(text.substr(start, end - start));
        }
        start = end + kDot.size();
    }
}

/* A text block's text, a line ended at each line break. */
void collect_block(
    const Rml::Element& element, std::string& piece, std::vector<std::string>& lines) {
    if (const auto* text = dynamic_cast<const Rml::ElementText*>(&element)) {
        piece += text->GetText();
        piece += ' ';
        return;
    }
    if (element.GetTagName() == "br") {
        end_line(piece, lines);
        return;
    }
    for (int i = 0; i < element.GetNumChildren(); ++i) {
        const Rml::Element* child = element.GetChild(i);
        if (child->IsVisible()) {
            collect_block(*child, piece, lines);
        }
    }
}

/* The lines of the headings and paragraphs inside the element. */
void collect_lines(const Rml::Element& element, std::vector<std::string>& lines) {
    if (!element.IsVisible()) {
        return;
    }
    const Rml::String& tag = element.GetTagName();
    if (tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "p") {
        std::string piece;
        collect_block(element, piece, lines);
        end_line(piece, lines);
        return;
    }
    for (int i = 0; i < element.GetNumChildren(); ++i) {
        collect_lines(*element.GetChild(i), lines);
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
    return spoken_value(control.value);
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

namespace {

/* A text field's value and its cursor in characters, -1 while text is
 * selected. Not a text field: empty, cursor at 0. */
struct FieldState {
    std::string value;
    int cursor = 0;
};

FieldState field_state(const Rml::Element& field) {
    const auto* input = dynamic_cast<const Rml::ElementFormControlInput*>(&field);
    if (input == nullptr) {
        return {};
    }
    int start = 0;
    int end = 0;
    input->GetSelection(&start, &end, nullptr);
    return FieldState{input->GetValue(), start == end ? start : -1};
}

/* A character as spoken: a screen reader says nothing for a lone space. */
std::string spoken_character(std::string character) {
    return character == " " ? "space" : character;
}

/* The character after the cursor, or blank at the end. */
std::string character_at(const std::string& value, int cursor) {
    const int begin = Rml::StringUtilities::ConvertCharacterOffsetToByteOffset(value, cursor);
    const int end = Rml::StringUtilities::ConvertCharacterOffsetToByteOffset(value, cursor + 1);
    return begin < end ? spoken_character(value.substr(begin, end - begin)) : kBlank;
}

}  // namespace

void FieldEcho::typed(std::string_view text) {
    m_typed.clear();
    for (const char c : text) {
        if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7f) {
            m_typed += c;
        }
    }
}

void FieldEcho::start(const Rml::Element& field) {
    FieldState state = field_state(field);
    m_value = std::move(state.value);
    m_cursor = state.cursor;
    m_typed.clear();
}

void FieldEcho::skip_cursor(const Rml::Element& field) {
    m_cursor = field_state(field).cursor;
}

std::optional<std::string> FieldEcho::frame(const Rml::Element& field) {
    FieldState state = field_state(field);
    const std::string typed = std::exchange(m_typed, {});
    if (state.value != m_value) {
        m_value = std::move(state.value);
        m_cursor = state.cursor;
        return spoken_value(m_value);
    }
    if (state.cursor != m_cursor) {
        /* Typing over a selection with the same text is still an edit. */
        const bool retyped = m_cursor < 0 && !typed.empty();
        m_cursor = state.cursor;
        if (retyped) {
            return spoken_value(m_value);
        }
        if (m_cursor >= 0) {
            return character_at(m_value, m_cursor);
        }
        return std::nullopt;
    }
    if (typed.empty()) {
        return std::nullopt;
    }
    const int limit = field.GetAttribute<int>("maxlength", -1);
    if (limit >= 0 && Rml::StringUtilities::LengthUTF8(m_value) >= static_cast<size_t>(limit)) {
        return "Full, " + std::to_string(limit) + " characters at most";
    }
    return spoken_character(typed) + " not allowed";
}

bool FieldEcho::changed_on_leaving(const Rml::Element& field) const {
    return field_state(field).value != m_value;
}

const Rml::Element* selected_tab(const Rml::ElementDocument& document) {
    Rml::ElementList tabs;
    const_cast<Rml::ElementDocument&>(document).GetElementsByClassName(tabs, "tab");
    for (const Rml::Element* tab : tabs) {
        if (tab->IsClassSet("selected")) {
            return tab;
        }
    }
    return nullptr;
}

std::vector<PageStop> tab_page_stops(const Rml::Element& tab) {
    std::vector<PageStop> stops{PageStop{&tab, {}}};
    const Rml::String& id = tab.GetId();
    if (id.rfind("tab-", 0) != 0) {
        return stops;
    }
    const Rml::Element* page = find_in_document(tab, "page-" + id.substr(4));
    for (int i = 0; page != nullptr && i < page->GetNumChildren(); ++i) {
        const Rml::Element* row = page->GetChild(i);
        if (const Rml::Element* control = first_control(*row)) {
            stops.push_back(PageStop{control, {}});
            continue;
        }
        std::vector<std::string> lines;
        collect_lines(*row, lines);
        for (std::string& line : lines) {
            stops.push_back(PageStop{nullptr, std::move(line)});
        }
    }
    return stops;
}

PageLines::Move PageLines::step(
    const std::vector<PageStop>& stops, const Rml::Element& focus, int direction) {
    const auto is_line = [&stops](int index) {
        return index >= 0 && index < static_cast<int>(stops.size()) &&
               stops[index].control == nullptr;
    };
    int at = -1;
    for (int i = 0; i < static_cast<int>(stops.size()); ++i) {
        if (stops[i].control == &focus) {
            at = i;
        }
    }
    if (at < 0) {
        clear();
        return Move::page;
    }
    /* A line that changed or went while it was held, as when the page's text
     * is rewritten, starts the press from focus again. */
    if (reading() && !(is_line(m_index) && stops[m_index].line == m_line)) {
        clear();
    }
    const int next = (reading() ? m_index : at) + direction;
    if (is_line(next)) {
        m_index = next;
        m_line = stops[next].line;
        return Move::line;
    }
    const bool was_reading = reading();
    clear();
    /* Off the lines: onto the control focus is still on, or past the lines,
     * where the page's own step from that control lands. */
    return was_reading && next == at ? Move::focus : Move::page;
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

namespace {

/* The percentage in a text such as "Verifying disc / 37%", and the text with
 * the number taken out. */
struct Percent {
    int value;
    std::string rest;
};

std::optional<Percent> find_percent(std::string_view text) {
    const size_t sign = text.rfind('%');
    if (sign == std::string_view::npos) {
        return std::nullopt;
    }
    size_t start = sign;
    while (start > 0 && text[start - 1] >= '0' && text[start - 1] <= '9') {
        --start;
    }
    int value = 0;
    if (std::from_chars(text.data() + start, text.data() + sign, value).ec != std::errc()) {
        return std::nullopt;
    }
    std::string rest(text.substr(0, start));
    rest += text.substr(sign);
    return Percent{value, std::move(rest)};
}

}  // namespace

std::optional<std::string> WatchedText::observe(const std::string& text, Clock::time_point now) {
    if (text == seen) {
        if (pending && now - since >= kSettleTime) {
            pending = false;
            if (std::find(resting.begin(), resting.end(), seen) == resting.end()) {
                return seen;
            }
        }
        return std::nullopt;
    }
    const std::optional<Percent> before = find_percent(seen);
    const std::optional<Percent> after = find_percent(text);
    seen = text;
    since = now;
    if (!before || !after || before->rest != after->rest) {
        progress_level = -1;
        pending = !text.empty();
        return std::nullopt;
    }
    /* Progress: spoken at each step, never after settling. The text that
     * started the run counts as reported, spoken or not. */
    pending = false;
    if (progress_level < 0) {
        progress_level = before->value / kProgressStep * kProgressStep;
    }
    const int reached = after->value >= 100 ? 100 : after->value / kProgressStep * kProgressStep;
    const bool crossed = reached > progress_level;
    progress_level = reached; /* also follows a run that starts over */
    if (crossed) {
        return text;
    }
    return std::nullopt;
}

std::optional<std::string> WatchedText::observe_in_view(
    const std::string& text, bool visible, Clock::time_point now) {
    const bool appeared = visible && !in_view;
    in_view = visible;
    if (visible && !appeared) {
        return observe(text, now);
    }
    seen = text;
    since = now;
    pending = false;
    progress_level = -1;
    return std::nullopt;
}

void SpokenOnce::add(std::optional<std::string> text) {
    if (text && std::find(m_texts.begin(), m_texts.end(), *text) == m_texts.end()) {
        m_texts.push_back(std::move(*text));
    }
}

PageReader::PageReader(Rml::ElementDocument& document, Speech& speech)
    : m_document(document), m_speech(speech) {}

void PageReader::typed(const Rml::Event& event) {
    if (event.GetTargetElement() == m_focus.get()) {
        m_field.typed(event.GetParameter<Rml::String>("text", ""));
    }
}

Control PageReader::describe(const Rml::Element& element) const {
    return describe_control(element);
}

void PageReader::add_to_focus_text(const Rml::Element&, std::string&) const {}

Rml::Element* PageReader::focused_control() const {
    Rml::Context* context = m_document.GetContext();
    Rml::Element* focus = context != nullptr ? context->GetFocusElement() : nullptr;
    if (focus == nullptr || focus->GetOwnerDocument() != &m_document || !is_control(*focus)) {
        return nullptr;
    }
    return focus;
}

void PageReader::follow_focus(Rml::Element* focus, std::string_view prefix) {
    if (focus == nullptr) {
        return;
    }
    if (focus != m_focus.get()) {
        m_lines.clear();
        std::string text(prefix);
        append_sentence(text, left_field_text());
        append_sentence(text, remember_focus(*focus));
        say(text, Mode::interrupt);
        return;
    }
    follow_value(*focus, describe(*focus));
}

/* A text field the page rewrote on being left, as the launcher puts back the
 * stored name when the field was left empty: its name and what it holds now.
 * Empty after any other control. */
std::string PageReader::left_field_text() const {
    const Rml::Element* left = m_focus.get();
    if (left == nullptr) {
        return "";
    }
    const Control field = describe(*left);
    if (field.role != Role::edit || !m_field.changed_on_leaving(*left)) {
        return "";
    }
    return field.name + ", " + field_value(field);
}

/* The focused control's value, spoken when it changes; a text field says
 * what its editing does. */
void PageReader::follow_value(const Rml::Element& focus, const Control& control) {
    if (control.role == Role::edit) {
        if (std::optional<std::string> text = m_field.frame(focus)) {
            m_lines.clear();
            say(*text, Mode::interrupt);
        }
        return;
    }
    if (control.value != m_focus_value) {
        m_lines.clear();
        m_focus_value = control.value;
        say(control.value, Mode::interrupt);
    }
}

std::string PageReader::remember_focus(Rml::Element& focus) {
    m_focus = focus.GetObserverPtr();
    m_focus_value = describe(focus).value;
    m_field.start(focus);
    return focus_text(focus);
}

void PageReader::forget_focus() {
    m_focus.reset();
    m_focus_value.clear();
    m_lines.clear();
}

std::string PageReader::focus_text(Rml::Element& focus) const {
    const Control control = describe(focus);
    std::string text = focus_announcement(control);
    /* Rows with no control, which focus never reaches, are read line by line
     * with Up and Down. A page that opens with them, as the launcher's
     * Controls page does, says so; on one that opens with a control, the
     * lines come in their turn between the controls. */
    if (control.role == Role::tab) {
        const std::vector<PageStop> stops = tab_page_stops(focus);
        if (stops.size() > 1 && stops[1].control == nullptr) {
            append_sentence(text, "Text below, Down to read");
        }
    }
    add_to_focus_text(focus, text);
    return text;
}

bool PageReader::step_lines(int direction, const Rml::Element* after_page) {
    Rml::Element* focus = focused_control();
    const Rml::Element* tab = selected_tab(m_document);
    if (!m_speech.enabled() || focus == nullptr || focus != m_focus.get() || tab == nullptr ||
        after_page == nullptr)
    {
        m_lines.clear();
        return false;
    }
    std::vector<PageStop> stops = tab_page_stops(*tab);
    stops.push_back(PageStop{after_page, {}});
    const PageLines::Move move = m_lines.step(stops, *focus, direction);
    /* Focus stays put: a text field has already moved its cursor to an end
     * for the press. */
    if (move != PageLines::Move::page) {
        m_field.skip_cursor(*focus);
    }
    switch (move) {
    case PageLines::Move::page:
        return false;
    case PageLines::Move::line:
        say(m_lines.line(), Mode::interrupt);
        return true;
    case PageLines::Move::focus:
        say(focus_text(*focus), Mode::interrupt);
        return true;
    }
    return false;
}

bool PageReader::displayed(const char* id) const {
    const Rml::Element* element = m_document.GetElementById(id);
    return element != nullptr && element->IsVisible(false);
}

std::string PageReader::text_of(const char* id) const {
    const Rml::Element* element = m_document.GetElementById(id);
    return element != nullptr ? element_text(*element) : std::string();
}

void PageReader::say(std::string_view text, Mode mode) {
    m_speech.announce(clean_text(text), mode);
}

bool is_document(const Rml::ElementDocument& document, std::string_view file) {
    const Rml::String& url = document.GetSourceURL();
    const size_t slash = url.find_last_of("/\\");
    return std::string_view(url).substr(slash == Rml::String::npos ? 0 : slash + 1) == file;
}

/* Tab and Shift+Tab become Down and Up, dispatched to the same element. */
void ReaderListeners::TabKeys::ProcessEvent(Rml::Event& event) {
    if (event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN) != Rml::Input::KI_TAB) {
        return;
    }
    Rml::Element* target = event.GetTargetElement();
    if (target == nullptr) {
        return;
    }
    const bool back = event.GetParameter<bool>("shift_key", false);
    /* Also skips RmlUi's default action, its own Tab navigation. */
    event.StopPropagation();
    Rml::Dictionary parameters = event.GetParameters();
    parameters["key_identifier"] = static_cast<int>(back ? Rml::Input::KI_UP : Rml::Input::KI_DOWN);
    parameters["shift_key"] = 0;
    target->DispatchEvent(Rml::EventId::Keydown, parameters);
}

void ReaderListeners::TypedText::ProcessEvent(Rml::Event& event) {
    if (reader != nullptr) {
        reader->typed(event);
    }
}

void ReaderListeners::attach(PageReader& reader) {
    reader.document().AddEventListener(Rml::EventId::Keydown, &m_tab_keys, true);
    reader.document().AddEventListener(Rml::EventId::Textinput, &m_typed_text, true);
    m_typed_text.reader = &reader;
}

void ReaderListeners::detach() {
    PageReader* reader = m_typed_text.reader;
    if (reader == nullptr) {
        return;
    }
    reader->document().RemoveEventListener(Rml::EventId::Keydown, &m_tab_keys, true);
    reader->document().RemoveEventListener(Rml::EventId::Textinput, &m_typed_text, true);
    m_typed_text.reader = nullptr;
}

}  // namespace a11y
