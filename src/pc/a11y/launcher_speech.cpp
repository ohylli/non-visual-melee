/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "launcher_speech.hpp"
#include "rmlui_reader.hpp"
#include "speech.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Plugin.h>
#include <RmlUi/Core/Core.h>
#include <algorithm>
#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {
namespace {

/* The opening waits for focus to land on a control, which it does once the
 * start-up disc check is over. If the page never focuses one, the opening is
 * spoken without it after this long. */
constexpr std::chrono::milliseconds kOpeningFocusWait{1000};

/* The fork's overrides, for controls whose shape the generic reader gets
 * wrong. These buttons sit in a setting row but act instead of cycling a
 * value, so they are read as "Performance preset: Apply, button". */
constexpr std::string_view kActionButtons[] = {"performance", "check-now", "settings-discord"};

Control describe(const Rml::Element& element) {
    Control control = describe_control(element);
    for (const std::string_view id : kActionButtons) {
        if (element.GetId() == id) {
            make_action(control);
        }
    }
    return control;
}

bool is_launcher(const Rml::ElementDocument& document) {
    const Rml::String& url = document.GetSourceURL();
    const size_t slash = url.find_last_of("/\\");
    return url.substr(slash == Rml::String::npos ? 0 : slash + 1) == "launcher.rml";
}

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

/* A region of text the reader speaks whenever it changes, under the settle
 * and progress rules. */
struct WatchedText {
    const char* id;
    const char* silent = nullptr; /* a text never spoken */
    std::string seen;             /* the text on the last frame, empty while hidden */
    Clock::time_point since;      /* when `seen` last changed */
    bool pending = false;         /* `seen` waits to settle before it is spoken */
    int progress_level = -1;      /* last step reached in a run of progress, -1 outside one */
    bool in_view = false;         /* for observe_in_view */

    /* Takes one frame's text; returns what to speak, if anything. */
    std::optional<std::string> observe(const std::string& text, Clock::time_point now) {
        if (text == seen) {
            if (pending && now - since >= kSettleTime) {
                pending = false;
                if (silent == nullptr || seen != silent) {
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
        const int reached =
            after->value >= 100 ? 100 : after->value / kProgressStep * kProgressStep;
        const bool crossed = reached > progress_level;
        progress_level = reached; /* also follows a run that starts over */
        if (crossed) {
            return text;
        }
        return std::nullopt;
    }

    /* For a region that is out of view at times: text that changes while it
     * is out of view, or together with its coming into view, is taken without
     * being spoken, as the view's own announcement covers it. */
    std::optional<std::string> observe_in_view(
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

    bool settled(Clock::time_point now) const { return now - since >= kSettleTime; }
};

/* Reads one launcher document for as long as it is loaded. */
class LauncherReader {
public:
    LauncherReader(Rml::ElementDocument& document, Speech& speech)
        : m_document(document), m_speech(speech) {}

    Rml::ElementDocument& document() const { return m_document; }

    void frame(Clock::time_point now) {
        Rml::Element* focus = focused_control();
        if (!m_opened) {
            open(focus, now);
            return;
        }
        const bool settings_shown = displayed("preferences");
        follow_focus(focus, settings_shown && !m_settings_shown);
        m_settings_shown = settings_shown;

        /* Queued in this order; a text two regions show at once, such as
         * "Checking for updates...", is spoken once. */
        std::vector<std::string> spoken;
        const auto add = [&spoken](std::optional<std::string> text) {
            if (text && std::find(spoken.begin(), spoken.end(), *text) == spoken.end()) {
                spoken.push_back(std::move(*text));
            }
        };
        add(m_status.observe(text_of(m_status.id), now));
        /* Watched while Settings is shown, whichever tab: an update check's
         * result still arrives after the player has moved on. */
        for (WatchedText* watch : {&m_settings_status, &m_check_status}) {
            add(watch->observe_in_view(text_of(watch->id), settings_shown, now));
        }
        /* The banner's own display, not whether Home is the view shown, so
         * coming back from Settings does not announce it again. */
        const bool update_shown = displayed("update-card");
        if (update_shown && !m_update_shown) {
            add("Update available: " + text_of("update-tag"));
        }
        m_update_shown = update_shown;
        for (WatchedText* watch : {&m_update_title, &m_update_desc}) {
            add(watch->observe(update_shown ? text_of(watch->id) : std::string(), now));
        }
        for (const std::string& text : spoken) {
            say(text, Mode::queue);
        }
    }

private:
    /* "Melee launcher. <status>. <focus announcement>", held until the status
     * line settles and focus reaches a control, so the start-up disc check is
     * heard as its result only. Queued behind the proof-of-life announcement. */
    void open(Rml::Element* focus, Clock::time_point now) {
        m_status.observe(text_of(m_status.id), now); /* the opening speaks it */
        if (!m_status.settled(now) ||
            (focus == nullptr && now - m_status.since < kSettleTime + kOpeningFocusWait))
        {
            return;
        }
        m_opened = true;
        m_status.pending = false;
        std::string text = "Melee launcher";
        append_sentence(text, m_status.seen);
        if (focus != nullptr) {
            append_sentence(text, remember_focus(*focus));
        }
        say(text, Mode::queue);
    }

    /* A new control gets its focus announcement; a new value on the same one
     * gets the value alone. Focus parked off any control, as while a button
     * is disabled, keeps the last control, so returning to it is silent. */
    void follow_focus(Rml::Element* focus, bool entering_settings) {
        if (focus == nullptr) {
            return;
        }
        if (focus != m_focus.get()) {
            std::string text = entering_settings ? "Settings" : "";
            /* A text field just left after typing: what it holds now, which
             * the launcher has put back to the stored value, so a rejected
             * entry is heard as the old value. */
            if (const Rml::Element* left = m_focus.get(); left != nullptr && m_edited) {
                if (const Control field = describe(*left); field.role == Role::edit) {
                    text = field.name + ", " + (field.value.empty() ? "blank" : field.value);
                }
            }
            append_sentence(text, remember_focus(*focus));
            say(text, Mode::interrupt);
            return;
        }
        const Control control = describe(*focus);
        /* Typed characters are not echoed yet. */
        if (control.role == Role::edit) {
            m_edited = m_edited || control.value != m_focus_value;
            return;
        }
        if (control.value != m_focus_value) {
            m_focus_value = control.value;
            say(control.value, Mode::interrupt);
        }
    }

    /* Makes `focus` the known control and returns its focus announcement. */
    std::string remember_focus(Rml::Element& focus) {
        const Control control = describe(focus);
        m_focus = focus.GetObserverPtr();
        m_focus_value = control.value;
        m_edited = false;
        std::string text = focus_announcement(control);
        /* Rows with no control, which focus never reaches, are read with
         * their tab: the connect code, the Controls page. */
        if (control.role == Role::tab) {
            append_sentence(text, tab_page_text(focus));
        }
        /* Play is the only way to hear the disc card, which focus never reaches. */
        if (focus.GetId() == "play") {
            std::string disc = text_of("disc-name");
            if (const std::string info = text_of("disc-info"); !info.empty()) {
                disc += ", " + info;
            }
            append_sentence(text, "Disc: " + disc);
        }
        return text;
    }

    Rml::Element* focused_control() const {
        Rml::Context* context = m_document.GetContext();
        Rml::Element* focus = context != nullptr ? context->GetFocusElement() : nullptr;
        if (focus == nullptr || focus->GetOwnerDocument() != &m_document || !is_control(*focus)) {
            return nullptr;
        }
        return focus;
    }

    /* Whether the element's own display shows it, whatever its ancestors do. */
    bool displayed(const char* id) const {
        const Rml::Element* element = m_document.GetElementById(id);
        return element != nullptr && element->IsVisible(false);
    }

    std::string text_of(const char* id) const {
        const Rml::Element* element = m_document.GetElementById(id);
        return element != nullptr ? element_text(*element) : std::string();
    }

    void say(std::string_view text, Mode mode) { m_speech.announce(clean_text(text), mode); }

    Rml::ElementDocument& m_document;
    Speech& m_speech;
    bool m_opened = false;
    Rml::ObserverPtr<Rml::Element> m_focus;
    std::string m_focus_value;
    bool m_edited = false; /* the focused text field's value has changed */
    bool m_update_shown = false;
    bool m_settings_shown = false;
    WatchedText m_status{"status"};
    WatchedText m_settings_status{"settings-status", "Saved automatically."};
    WatchedText m_check_status{"check-status"};
    WatchedText m_update_title{"update-title"};
    WatchedText m_update_desc{"update-desc"};
};

/* Tab and Shift+Tab become Down and Up, so both keys follow the launcher's own
 * order instead of RmlUi's document order. Listens in the capture phase: the
 * launcher's own listener stops the arrow keys before they bubble. */
class TabKeys final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        if (event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN) != Rml::Input::KI_TAB)
        {
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
        parameters["key_identifier"] =
            static_cast<int>(back ? Rml::Input::KI_UP : Rml::Input::KI_DOWN);
        parameters["shift_key"] = 0;
        target->DispatchEvent(Rml::EventId::Keydown, parameters);
    }
};

/* EVT_BASIC is there for RmlUi's shutdown, which unregisters only the plugins
 * in that class. Static, because RmlUi keeps the pointer until it shuts down. */
class LauncherPlugin final : public Rml::Plugin {
public:
    int GetEventClasses() override { return EVT_BASIC | EVT_DOCUMENT; }

    void OnDocumentLoad(Rml::ElementDocument* document) override {
        if (m_speech == nullptr || m_reader != nullptr || !is_launcher(*document)) {
            return;
        }
        document->AddEventListener(Rml::EventId::Keydown, &m_tab_keys, true);
        m_reader = std::make_unique<LauncherReader>(*document, *m_speech);
    }

    void OnDocumentUnload(Rml::ElementDocument* document) override {
        if (m_reader != nullptr && &m_reader->document() == document) {
            detach();
        }
    }

    /* By now RmlUi has unloaded every document, and with it the reader. */
    void OnShutdown() override { m_reader.reset(); }

    void start(Speech& speech) {
        m_speech = &speech;
        if (!m_registered) {
            Rml::RegisterPlugin(this);
            m_registered = true;
        }
    }

    void stop() {
        detach();
        if (m_registered) {
            Rml::UnregisterPlugin(this);
            m_registered = false;
        }
        m_speech = nullptr;
    }

    void frame(Clock::time_point now) {
        if (m_reader != nullptr) {
            m_reader->frame(now);
        }
    }

private:
    void detach() {
        if (m_reader == nullptr) {
            return;
        }
        m_reader->document().RemoveEventListener(Rml::EventId::Keydown, &m_tab_keys, true);
        m_reader.reset();
    }

    Speech* m_speech = nullptr;
    bool m_registered = false;
    std::unique_ptr<LauncherReader> m_reader;
    TabKeys m_tab_keys;
};

LauncherPlugin s_plugin;

}  // namespace

void launcher_speech_start(Speech& speech) {
    s_plugin.start(speech);
}

void launcher_speech_stop() {
    s_plugin.stop();
}

void launcher_speech_frame(Clock::time_point now) {
    s_plugin.frame(now);
}

}  // namespace a11y
