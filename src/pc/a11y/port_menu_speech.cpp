/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "port_menu_speech.hpp"
#include "rmlui_reader.hpp"
#include "speech.hpp"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <cctype>
#include <string>
#include <string_view>

namespace a11y {
namespace {

/* The menu focuses its category as it opens. If no control has focus by
 * then, the opening is spoken without it after this long. */
constexpr std::chrono::milliseconds kOpeningFocusWait{1000};

/* What the opening calls the menu. The page names itself only in its title
 * bar, "Melee PC / Settings". */
constexpr const char* kHeading = "PC settings";

/* Whether the element is one of the Controls page's binding buttons: the
 * numbered "bind-" and "axis-" ids, and "menu-binding". Their label is the
 * binding, which is what a value is, though the page marks them as buttons
 * that act; the remapping feature decides their words. */
bool is_binding(const Rml::Element& element) {
    const Rml::String& id = element.GetId();
    for (const std::string_view prefix : {"bind-", "axis-"}) {
        if (id.size() > prefix.size() && id.compare(0, prefix.size(), prefix) == 0 &&
            std::isdigit(static_cast<unsigned char>(id[prefix.size()])))
        {
            return true;
        }
    }
    return id == "menu-binding";
}

/* Reads the port menu's document for as long as it is loaded, which is from
 * the game's start to its end; it speaks only while the menu is open. */
class PortMenuReader final : public PageReader {
public:
    PortMenuReader(Rml::ElementDocument& document, Speech& speech) : PageReader(document, speech) {}

    void frame(bool open, bool quitting, Clock::time_point now) {
        if (!open) {
            if (m_open) {
                m_open = false;
                forget_focus();
                /* Interrupts, so leftover menu speech stops as the game
                 * resumes, or as it shuts down after Quit game. */
                say(quitting ? "Quitting the game" : "Back to game", Mode::interrupt);
                /* Out of view until the next opening, which takes them
                 * afresh; nothing to read while closed. */
                observe_regions(nullptr, false, now);
            }
            return;
        }
        Rml::Element* focus = focused_control();
        if (!m_open) {
            m_open = true;
            m_opening = true;
            m_opened_at = now;
        }
        if (m_opening) {
            if (focus == nullptr && now - m_opened_at < kOpeningFocusWait) {
                observe_regions(focus, false, now);
                return;
            }
            say_opening(focus);
            /* The menu rewrote both regions as it opened: taken silently. */
            observe_regions(focus, true, now);
            return;
        }
        follow_focus(focus, "");
        const SpokenOnce spoken = observe_regions(focus, true, now);
        for (const std::string& text : spoken.texts()) {
            say(text, Mode::queue);
        }
    }

    /* Up or Down, before the menu moves focus: over the lines of the open
     * category's page between its rows. */
    bool step(int direction) {
        if (!m_open || m_opening) {
            stop_reading_lines();
            return false;
        }
        /* The menu's column of rows (PortMenu::show_tab) does not wrap: past
         * the last row it moves on to Resume, before the first to the
         * category. */
        return step_lines(direction, false);
    }

private:
    void adjust(const Rml::Element& element, Control& control) const override {
        if (is_binding(element)) {
            control.role = Role::binding;
        }
        /* A category's description only repeats its name: "Graphics
         * settings.". Check for updates' is the result the page showed
         * first, which it never rewrites; the row's live text says the
         * current one. */
        if (control.role == Role::category || element.GetId() == "port-check-update") {
            control.help.clear();
        }
    }

    /* "PC settings. <the focused category's announcement, with how to
     * move>", interrupting: the menu cuts into whatever the game was saying.
     * The menu opens on the category used last. */
    void say_opening(Rml::Element* focus) {
        m_opening = false;
        std::string text = kHeading;
        if (focus != nullptr) {
            append_sentence(text, remember_focus(*focus, true));
        }
        say(text, Mode::interrupt);
    }

    /* The status line and the update check's result, in view while the menu
     * is open on any category, so a check's result is heard after the player
     * has moved on. "Saved.", which follows every change, is never spoken,
     * nor what the regions show as the menu opens, nor the description the
     * status line shows for the focused control, which its announcement
     * has said. */
    SpokenOnce observe_regions(const Rml::Element* focus, bool in_view, Clock::time_point now) {
        m_status.echo = focus != nullptr ? page_description(*focus) : std::string();
        SpokenOnce spoken;
        for (WatchedText* watch : {&m_status, &m_update_status}) {
            spoken.add(watch->observe_in_view(text_of(watch->id), in_view, now));
        }
        return spoken;
    }

    bool m_open = false;
    bool m_opening = false; /* open, the opening not yet spoken */
    Clock::time_point m_opened_at;
    WatchedText m_status{"menu-status", {"Saved."}};
    WatchedText m_update_status{"port-update-status"};
};

ReaderPlugin<PortMenuReader> s_plugin{"port-menu.rml"};

}  // namespace

void port_menu_speech_start(Speech& speech) {
    s_plugin.start(speech);
}

void port_menu_speech_stop() {
    s_plugin.stop();
}

void port_menu_speech_frame(bool open, bool quitting, Clock::time_point now) {
    if (PortMenuReader* reader = s_plugin.reader()) {
        reader->frame(open, quitting, now);
    }
}

bool port_menu_speech_step(int direction) {
    PortMenuReader* reader = s_plugin.reader();
    return reader != nullptr && reader->step(direction);
}

}  // namespace a11y
