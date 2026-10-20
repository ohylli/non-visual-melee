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

/* `skip_detail`: leaves out the live text a row's name holds (class
 * "detail"), which is read on its own. */
void collect_text(const Rml::Element& element, std::string& out, bool skip_detail = false) {
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
        if (child->IsVisible() && !(skip_detail && child->IsClassSet("detail"))) {
            collect_text(*child, out, skip_detail);
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

/* A row of a settings page: a name (class "name") beside a control, or
 * beside text alone. */
const Rml::Element* row_of(const Rml::Element& element) {
    for (const Rml::Element* e = element.GetParentNode(); e != nullptr; e = e->GetParentNode()) {
        if (e->IsClassSet("row")) {
            return e;
        }
    }
    return nullptr;
}

/* The first descendant with the class, or null. */
const Rml::Element* first_of_class(const Rml::Element& element, const char* name) {
    Rml::ElementList found;
    const_cast<Rml::Element&>(element).GetElementsByClassName(found, name);
    return found.empty() ? nullptr : found.front();
}

/* The row's name, with a badge such as "RESTART" in it, without its live
 * text. */
std::string row_name(const Rml::Element& row) {
    const Rml::Element* name = first_of_class(row, "name");
    if (name == nullptr) {
        return "";
    }
    std::string text;
    collect_text(*name, text, true);
    return collapse_whitespace(text);
}

/* The row's live text: a detail span the page rewrites, which has an id
 * ("No controller in this port.", the update check's result). A detail
 * without one only repeats the row's description. */
std::string row_detail(const Rml::Element& row) {
    Rml::ElementList found;
    const_cast<Rml::Element&>(row).GetElementsByClassName(found, "detail");
    std::string text;
    for (const Rml::Element* detail : found) {
        const std::string part = detail->GetId().empty() ? "" : element_text(*detail);
        if (!part.empty()) {
            text += text.empty() ? part : ", " + part;
        }
    }
    return text;
}

std::string lowercase(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

/* A category of the settings sidebar: a button "tab-x" in #categories. */
bool is_category(const Rml::Element& element) {
    const Rml::Element* parent = element.GetParentNode();
    return element.GetTagName() == "button" && element.GetId().rfind("tab-", 0) == 0 &&
           parent != nullptr && parent->GetId() == "categories";
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

/* Whether a description says something the name and the live text do not:
 * the pages fill some with the row's name ("Display mode") or with "-". */
bool adds_to(std::string_view help, const Control& control) {
    while (!help.empty() && help.back() == '.') {
        help.remove_suffix(1);
    }
    if (help.empty() || help == "-") {
        return false;
    }
    const std::string said = lowercase(control.name + " " + control.detail);
    return said.find(lowercase(help)) == std::string::npos;
}

/* The line of a row that has no control: its name and what it shows. */
std::string row_line(const Rml::Element& row) {
    std::string line = row_name(row);
    const auto add = [&line](const std::string& part) {
        if (!part.empty()) {
            line += line.empty() ? part : ", " + part;
        }
    };
    add(row_detail(row));
    for (int i = 0; i < row.GetNumChildren(); ++i) {
        const Rml::Element* child = row.GetChild(i);
        if (child->IsVisible() && !child->IsClassSet("name")) {
            add(element_text(*child));
        }
    }
    return line;
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

std::string page_description(const Rml::Element& element) {
    for (const Rml::Element* e = &element; e != nullptr; e = e->GetParentNode()) {
        const Rml::String desc = e->GetAttribute<Rml::String>("data-desc", "");
        if (!desc.empty()) {
            return collapse_whitespace(desc);
        }
    }
    return "";
}

Control describe_control(const Rml::Element& element) {
    Control control;
    control.disabled = element.IsPseudoClassSet("disabled");
    if (const Rml::Element* row = row_of(element)) {
        control.name = row_name(*row);
        control.detail = row_detail(*row);
    }
    const Rml::String& tag = element.GetTagName();
    if (tag == "button") {
        control.value = element_text(element);
        if (is_category(element)) {
            control.role = Role::category;
            control.name = control.value;
        } else if (control.name.empty()) {
            control.role = Role::button;
            control.name = control.value;
        } else if (element.IsClassSet("action")) {
            control.role = Role::button;
        } else {
            control.role = Role::setting;
        }
    } else {
        control.value = form_value(element);
        if (tag == "input") {
            const Rml::String type = element.GetAttribute<Rml::String>("type", "text");
            if (type == "range") {
                control.role = Role::slider;
                /* The span beside a slider shows its value as the player reads
                 * it ("80%", "Auto"); the slider's own value is a bare number. */
                if (const Rml::Element* shown = find_in_document(element, element.GetId() + "-val"))
                {
                    control.value = element_text(*shown);
                }
            } else if (type == "text" || type == "password") {
                control.role = Role::edit;
            }
        }
    }
    if (std::string help = page_description(element); adds_to(help, control)) {
        control.help = std::move(help);
    }
    return control;
}

std::string field_value(const Control& control) {
    return spoken_value(control.value);
}

std::string focus_announcement(const Control& control, bool explain) {
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
    std::string_view keys;  /* after a comma: "Vertical sync, On, Left and right to change" */
    std::string_view moves; /* a sentence of its own: "Left and right to adjust" */
    switch (control.role) {
    case Role::button:
        /* A row's button that acts: "Performance preset: Apply". */
        if (control.value != control.name && !control.value.empty()) {
            text += ": " + control.value;
        }
        add("button");
        break;
    case Role::setting:
        add(control.value);
        keys = "Left and right to change";
        break;
    case Role::binding:
        add(control.value);
        keys = "Enter to change";
        break;
    case Role::slider:
        add("slider");
        add(control.value);
        moves = "Left and right to adjust";
        break;
    case Role::edit:
        add("edit");
        add(field_value(control));
        break;
    case Role::category:
        if (explain) {
            moves = "Up and down to choose, right to enter";
        }
        break;
    case Role::other:
        if (control.value != control.name) {
            add(control.value);
        }
        break;
    }
    if (control.disabled) {
        add("unavailable");
    } else {
        add(keys);
        append_sentence(text, moves);
    }
    append_sentence(text, control.detail);
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

const Rml::Element* selected_category(const Rml::ElementDocument& document) {
    const Rml::Element* sidebar =
        const_cast<Rml::ElementDocument&>(document).GetElementById("categories");
    for (int i = 0; sidebar != nullptr && i < sidebar->GetNumChildren(); ++i) {
        const Rml::Element* category = sidebar->GetChild(i);
        if (is_category(*category) && category->IsClassSet("selected")) {
            return category;
        }
    }
    return nullptr;
}

const Rml::Element* category_page(const Rml::Element& category) {
    const Rml::String& id = category.GetId();
    return id.rfind("tab-", 0) == 0 ? find_in_document(category, "page-" + id.substr(4)) : nullptr;
}

std::vector<PageStop> page_stops(const Rml::Element& page) {
    std::vector<PageStop> stops;
    for (int i = 0; i < page.GetNumChildren(); ++i) {
        const Rml::Element* row = page.GetChild(i);
        if (!row->IsVisible()) {
            continue;
        }
        if (const Rml::Element* control = first_control(*row)) {
            stops.push_back(PageStop{control, {}});
            continue;
        }
        std::vector<std::string> lines;
        if (first_of_class(*row, "name") != nullptr) {
            lines.push_back(row_line(*row));
        } else {
            collect_lines(*row, lines);
        }
        for (std::string& line : lines) {
            if (!line.empty()) {
                stops.push_back(PageStop{nullptr, std::move(line)});
            }
        }
    }
    return stops;
}

PageLines::Move PageLines::step(
    const std::vector<PageStop>& stops, const Rml::Element& focus, int direction, bool wrap) {
    const int count = static_cast<int>(stops.size());
    const auto is_line = [&stops, count](int index) {
        return index >= 0 && index < count && stops[index].control == nullptr;
    };
    int at = -1;
    for (int i = 0; i < count && at < 0; ++i) {
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
    int next = (reading() ? m_index : at) + direction;
    if (wrap) {
        next = (next + count) % count;
    }
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
        pending = !text.empty() && text != echo;
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
    Control control = describe_control(element);
    adjust(element, control);
    return control;
}

void PageReader::adjust(const Rml::Element&, Control&) const {}

void PageReader::add_to_focus_text(const Rml::Element&, std::string&) const {}

Rml::Element* PageReader::focused_control() const {
    Rml::Context* context = m_document.GetContext();
    Rml::Element* focus = context != nullptr ? context->GetFocusElement() : nullptr;
    if (focus == nullptr || focus->GetOwnerDocument() != &m_document || !is_control(*focus)) {
        return nullptr;
    }
    return focus;
}

void PageReader::follow_focus(Rml::Element* focus, std::string_view prefix, bool explain) {
    if (focus == nullptr) {
        return;
    }
    if (focus != m_focus.get()) {
        stop_reading_lines();
        std::string text(prefix);
        append_sentence(text, left_field_text());
        append_sentence(text, remember_focus(*focus, explain));
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
        std::string text = control.value;
        /* Live text that changed with the value says what the value now
         * means: "Port 2, No controller in this port.". Changing alone, it is
         * the page's news, which the page's watched regions speak. */
        if (control.detail != m_focus_detail && !control.detail.empty()) {
            text += ", " + control.detail;
        }
        say(text, Mode::interrupt);
    }
    m_focus_detail = control.detail;
}

std::string PageReader::remember_focus(Rml::Element& focus, bool explain) {
    m_focus = focus.GetObserverPtr();
    const Control control = describe(focus);
    m_focus_value = control.value;
    m_focus_detail = control.detail;
    m_field.start(focus);
    return focus_text(focus, explain);
}

void PageReader::forget_focus() {
    m_focus.reset();
    m_focus_value.clear();
    m_focus_detail.clear();
    stop_reading_lines();
}

namespace {

bool has_control(const std::vector<PageStop>& stops) {
    return std::any_of(
        stops.begin(), stops.end(), [](const PageStop& stop) { return stop.control != nullptr; });
}

}  // namespace

std::string PageReader::focus_text(Rml::Element& focus, bool explain) const {
    std::string text = focus_announcement(describe(focus), explain);
    add_to_focus_text(focus, text);
    return text;
}

const Rml::Element* PageReader::open_page() const {
    const Rml::Element* category = selected_category(m_document);
    return category != nullptr ? category_page(*category) : nullptr;
}

std::vector<PageStop> PageReader::text_alone(const Rml::Element& focus) const {
    const Rml::Element* page = open_page();
    if (!m_speech.enabled() || &focus != selected_category(m_document) || &focus != m_focus.get() ||
        page == nullptr)
    {
        return {};
    }
    std::vector<PageStop> stops = page_stops(*page);
    return has_control(stops) ? std::vector<PageStop>{} : stops;
}

void PageReader::enter_text() {
    const Rml::Element* focus = focused_control();
    const std::vector<PageStop> stops =
        focus != nullptr ? text_alone(*focus) : std::vector<PageStop>{};
    if (stops.empty()) {
        m_text_line = -1;
        return;
    }
    /* Pressed again while reading, the line being read. */
    m_text_line = std::clamp(m_text_line, 0, static_cast<int>(stops.size()) - 1);
    say(stops[m_text_line].line, Mode::interrupt);
}

bool PageReader::leave_text() {
    Rml::Element* focus = focused_control();
    if (m_text_line < 0 || focus == nullptr || text_alone(*focus).empty()) {
        m_text_line = -1;
        return false;
    }
    m_text_line = -1;
    say(focus_text(*focus), Mode::interrupt);
    return true;
}

bool PageReader::step_lines(int direction, bool wrap) {
    Rml::Element* focus = focused_control();
    const Rml::Element* page = open_page();
    if (!m_speech.enabled() || focus == nullptr || focus != m_focus.get() || page == nullptr) {
        stop_reading_lines();
        return false;
    }
    /* A page of text alone, entered: its lines are the column, wrapping as
     * the page's rows do, and focus stays on its category. */
    if (m_text_line >= 0) {
        const std::vector<PageStop> lines = text_alone(*focus);
        if (!lines.empty()) {
            const int count = static_cast<int>(lines.size());
            m_text_line = std::min(m_text_line, count - 1) + direction;
            m_text_line =
                wrap ? (m_text_line + count) % count : std::clamp(m_text_line, 0, count - 1);
            say(lines[m_text_line].line, Mode::interrupt);
            return true;
        }
        m_text_line = -1;
    }
    const std::vector<PageStop> stops = page_stops(*page);
    const PageLines::Move move = m_lines.step(stops, *focus, direction, wrap);
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
