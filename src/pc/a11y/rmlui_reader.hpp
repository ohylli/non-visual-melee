/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Generic reading of an RmlUi page: what a control is called, what it is and
 * what value it shows, all taken from the page's own text, and the reader
 * every page's speech is built on (PageReader, ReaderPlugin). Knows nothing
 * about any one page; the launcher's specifics live in launcher_speech.cpp,
 * the port menu's in port_menu_speech.cpp. docs/a11y/rmlui-screens.md
 * explains it. */
#pragma once
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Plugin.h>
#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

class Speech;
enum class Mode;

using Clock = std::chrono::steady_clock;

/* The two numbers to tune by ear. A watched text is spoken once it has stayed
 * the same this long, so passing messages are skipped. */
constexpr std::chrono::milliseconds kSettleTime{250};
/* A text whose only change is its percentage is progress, spoken each time it
 * crosses the next multiple of this, and at 100%. */
constexpr int kProgressStep = 20;

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
 * preset", "Apply"). The page does not mark them, so each page's reader
 * lists them by id: its name becomes "Performance preset: Apply". */
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

/* What a focused text field says while it is edited, found by comparing it
 * with the last frame. A new value is spoken whole ("O", "OT", "OTT"); a
 * cursor move says the character after the cursor, the one Delete removes, or
 * "blank" at the end; typing that leaves the field as it was is "Full, <n>
 * characters at most" in a field at its maxlength and "# not allowed"
 * otherwise, as the page refused the character. Moves while text is selected
 * are not read. Any element that is not a text field reads as an empty one. */
class FieldEcho {
public:
    /* Starts following a field that just got focus. */
    void start(const Rml::Element& field);
    /* Text typed while the field has focus, noted before the field takes it.
     * Control characters are left out: Enter types a line break, which a
     * one-line field never takes. */
    void typed(std::string_view text);
    /* Takes the field's cursor without speaking it, for a move the page
     * made itself rather than the player: Up and Down in a field move its
     * cursor to an end, though they are read as leaving it. */
    void skip_cursor(const Rml::Element& field);
    /* Once a frame while the field has focus: what to say, if anything. */
    std::optional<std::string> frame(const Rml::Element& field);
    /* Whether the field, just left, now holds something other than it did
     * on the last frame: the page wrote its stored value back. */
    bool changed_on_leaving(const Rml::Element& field) const;

private:
    std::string m_value;
    int m_cursor = 0; /* in characters, -1 while text is selected */
    std::string m_typed;
};

/* A region of text a reader speaks whenever it changes, under the settle and
 * progress rules (docs/a11y/rmlui-screens.md, "Why the reader compares
 * frames"). */
struct WatchedText {
    const char* id;
    /* The region's idle texts, never spoken: "Saved automatically." */
    std::vector<std::string_view> resting{};
    std::string seen{};        /* the text on the last frame, empty while hidden */
    Clock::time_point since{}; /* when `seen` last changed */
    bool pending = false;      /* `seen` waits to settle before it is spoken */
    int progress_level = -1;   /* last step reached in a run of progress, -1 outside one */
    bool in_view = false;      /* the region was in view on the last frame */

    /* Takes one frame's text; returns what to speak, if anything. */
    std::optional<std::string> observe(const std::string& text, Clock::time_point now);
    /* For a region that is out of view at times: text that changes while it
     * is out of view, or together with its coming into view, is taken without
     * being spoken, as the view's own announcement covers it. */
    std::optional<std::string> observe_in_view(
        const std::string& text, bool visible, Clock::time_point now);
    bool settled(Clock::time_point now) const { return now - since >= kSettleTime; }
};

/* Queues each text once, in order: a text two regions show in the same frame,
 * such as "Checking for updates...", is spoken once. */
class SpokenOnce {
public:
    void add(std::optional<std::string> text);
    const std::vector<std::string>& texts() const { return m_texts; }

private:
    std::vector<std::string> m_texts;
};

/* What every page's reader shares, from comparing the page with the last
 * frame: the focused control's announcement and value, a focused text
 * field's echo, and the lines between the controls of a tab's page. A page's
 * own reader adds its opening, its watched regions and its focus order, and
 * calls these from its frame and step. */
class PageReader {
public:
    /* `action_buttons`: the ids of the page's setting-row buttons that act
     * rather than cycle (make_action), a table that outlives the reader. */
    PageReader(Rml::ElementDocument& document, Speech& speech,
        std::span<const std::string_view> action_buttons);
    virtual ~PageReader() = default;
    PageReader(const PageReader&) = delete;
    PageReader& operator=(const PageReader&) = delete;

    Rml::ElementDocument& document() const { return m_document; }
    /* Text typed into the page, before the field or the page acts on it. */
    void typed(const Rml::Event& event);

protected:
    /* What the page adds to a control's focus announcement, if anything. */
    virtual void add_to_focus_text(const Rml::Element& focus, std::string& text) const;

    /* The control with focus in this document, or null while focus is parked
     * off any control. */
    Rml::Element* focused_control() const;
    /* A new control gets its focus announcement, after `prefix`, interrupting;
     * a new value on the same one gets the value alone. Focus parked off any
     * control keeps the last control, so returning to it is silent. Either
     * change ends reading the page's lines. */
    void follow_focus(Rml::Element* focus, std::string_view prefix);
    /* Makes `focus` the known control and returns its focus announcement. */
    std::string remember_focus(Rml::Element& focus);
    /* No control known: the next focus is announced, whatever it is. */
    void forget_focus();
    /* The control's announcement, with the hint that a tab's page opens
     * with lines, and what the page adds. */
    std::string focus_text(Rml::Element& focus) const;
    /* Up (-1) or Down (1), before the page moves focus: over the selected
     * tab's page, whose stops `after_page` follows in the page's own order.
     * Speaks what the step lands on and returns true when the press was used,
     * so focus must stay put. Leaves every press to the page while the
     * accessibility switch is off. */
    bool step_lines(int direction, const Rml::Element* after_page);
    void stop_reading_lines() { m_lines.clear(); }

    /* Whether the element's own display shows it, whatever its ancestors do. */
    bool displayed(const char* id) const;
    std::string text_of(const char* id) const;
    /* Through clean_text. */
    void say(std::string_view text, Mode mode);

private:
    /* A control as the page's reader hears it: describe_control, with the
     * page's action buttons made actions. */
    Control describe(const Rml::Element& element) const;
    std::string left_field_text() const;
    void follow_value(const Rml::Element& focus, const Control& control);

    Rml::ElementDocument& m_document;
    Speech& m_speech;
    std::span<const std::string_view> m_action_buttons;
    Rml::ObserverPtr<Rml::Element> m_focus;
    std::string m_focus_value;
    FieldEcho m_field; /* follows the focused control as a text field */
    PageLines m_lines;
};

/* Whether the document was loaded from a file of this name. */
bool is_document(const Rml::ElementDocument& document, std::string_view file);

/* The listeners every reader needs on its document, in the capture phase.
 * Tab and Shift+Tab become Down and Up, so both keys follow the page's own
 * order instead of RmlUi's document order; capture, because the pages' own
 * listeners stop the arrow keys before they bubble. Typed text reaches the
 * reader ahead of the page's own filter and the field, either of which may
 * take it without changing the field. */
class ReaderListeners {
public:
    void attach(PageReader& reader);
    void detach();
    /* Drops the reader without touching its document, which RmlUi has
     * already destroyed. */
    void forget() { m_typed_text.reader = nullptr; }

private:
    class TabKeys final : public Rml::EventListener {
    public:
        void ProcessEvent(Rml::Event& event) override;
    };
    class TypedText final : public Rml::EventListener {
    public:
        void ProcessEvent(Rml::Event& event) override;
        PageReader* reader = nullptr;
    };

    TabKeys m_tab_keys;
    TypedText m_typed_text;
};

/* An RmlUi plugin that starts a reader when its document loads and drops it
 * when the document unloads. RmlUi keeps a pointer to a registered plugin
 * until it shuts down, so a plugin is a static object. EVT_BASIC is there
 * for RmlUi's shutdown, which unregisters only the plugins in that class. */
template <class Reader>
class ReaderPlugin final : public Rml::Plugin {
public:
    explicit ReaderPlugin(std::string_view file) : m_file(file) {}

    int GetEventClasses() override { return EVT_BASIC | EVT_DOCUMENT; }

    void OnDocumentLoad(Rml::ElementDocument* document) override {
        if (m_speech == nullptr || m_reader != nullptr || !is_document(*document, m_file)) {
            return;
        }
        m_reader = std::make_unique<Reader>(*document, *m_speech);
        m_listeners.attach(*m_reader);
    }

    void OnDocumentUnload(Rml::ElementDocument* document) override {
        if (m_reader != nullptr && &m_reader->document() == document) {
            detach();
        }
    }

    /* By now RmlUi has unloaded every document, and with it the reader. */
    void OnShutdown() override {
        m_listeners.forget();
        m_reader.reset();
    }

    /* Registers the plugin; speech must outlive the matching stop. Call
     * before or after RmlUi starts. */
    void start(Speech& speech) {
        m_speech = &speech;
        if (!m_registered) {
            Rml::RegisterPlugin(this);
            m_registered = true;
        }
    }

    /* Unregisters the plugin and drops the reader. RmlUi may shut down
     * before or after. */
    void stop() {
        detach();
        if (m_registered) {
            Rml::UnregisterPlugin(this);
            m_registered = false;
        }
        m_speech = nullptr;
    }

    /* The document's reader while it is loaded, else null. */
    Reader* reader() const { return m_reader.get(); }

private:
    void detach() {
        if (m_reader == nullptr) {
            return;
        }
        m_listeners.detach();
        m_reader.reset();
    }

    std::string_view m_file;
    Speech* m_speech = nullptr;
    bool m_registered = false;
    std::unique_ptr<Reader> m_reader;
    ReaderListeners m_listeners;
};

/* Turns the page's visual separators into commas: "Ready to play / Disc not
 * verified." reads "Ready to play, Disc not verified.". Applied to a whole
 * announcement just before it is spoken. */
std::string clean_text(std::string_view text);

/* Appends a sentence to an announcement, adding ". " unless the text so far
 * already ends a sentence. */
void append_sentence(std::string& announcement, std::string_view sentence);

}  // namespace a11y
