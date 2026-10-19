/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "port_menu_speech.hpp"
#include "rmlui_reader.hpp"
#include "speech.hpp"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <string>
#include <string_view>

namespace a11y {
namespace {

/* The menu focuses its tab as it opens. If no control has focus by then,
 * the opening is spoken without it after this long. */
constexpr std::chrono::milliseconds kOpeningFocusWait{1000};

/* The fork's overrides, for controls whose shape the generic reader gets
 * wrong. These buttons sit in a setting row but act instead of cycling a
 * value, so they are read as "Check for updates: Check now, button". The
 * Controls tab's binding buttons act too, but their label is the binding,
 * which is what a value is; the remapping feature decides their words. */
constexpr std::string_view kActionButtons[] = {"port-check-update", "bind-reset"};

/* Reads the port menu's document for as long as it is loaded, which is from
 * the game's start to its end; it speaks only while the menu is open. */
class PortMenuReader final : public PageReader {
public:
    PortMenuReader(Rml::ElementDocument& document, Speech& speech) : PageReader(document, speech) {}

    void frame(bool open, Clock::time_point now) {
        if (!open) {
            if (m_open) {
                m_open = false;
                forget_focus();
                /* Interrupts, so leftover menu speech stops as the game
                 * resumes. */
                say("Back to game", Mode::interrupt);
            }
            observe_regions(false, now);
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
                observe_regions(false, now);
                return;
            }
            say_opening(focus);
            /* The menu rewrote both regions as it opened: taken silently. */
            observe_regions(true, now);
            return;
        }
        follow_focus(focus, "");
        for (const std::string& text : observe_regions(true, now).texts()) {
            say(text, Mode::queue);
        }
    }

    /* Up or Down, before the menu moves focus: over the lines of the
     * selected tab's page between the controls. */
    bool step(int direction) {
        if (!m_open || m_opening) {
            stop_reading_lines();
            return false;
        }
        /* The menu's own order (PortMenu::show_tab): the tab, the page's
         * controls, then Return to game. */
        return step_lines(direction, document().GetElementById("resume"));
    }

private:
    Control describe(const Rml::Element& element) const override {
        Control control = describe_control(element);
        for (const std::string_view id : kActionButtons) {
            if (element.GetId() == id) {
                make_action(control);
            }
        }
        return control;
    }

    /* "PC settings. <the focused tab's announcement>", interrupting: the menu
     * cuts into whatever the game was saying. The menu opens on the tab used
     * last. */
    void say_opening(Rml::Element* focus) {
        m_opening = false;
        std::string text = heading();
        if (focus != nullptr) {
            append_sentence(text, remember_focus(*focus));
        }
        say(text, Mode::interrupt);
    }

    /* The page's heading, "PC settings". */
    std::string heading() const {
        Rml::Element* block = document().GetElementById("heading");
        Rml::ElementList found;
        if (block != nullptr) {
            block->GetElementsByTagName(found, "h1");
        }
        return found.empty() ? std::string() : element_text(*found.front());
    }

    /* The status line and the update check's result, in view while the menu
     * is open on any tab, so a check's result is heard after the player has
     * moved on. "Saved.", which follows every change, is never spoken, nor
     * what the regions show as the menu opens or while it is closed. */
    SpokenOnce observe_regions(bool in_view, Clock::time_point now) {
        SpokenOnce spoken;
        for (WatchedText* watch : {&m_status, &m_update_status}) {
            spoken.add(watch->observe_in_view(text_of(watch->id), in_view, now));
        }
        return spoken;
    }

    bool m_open = false;
    bool m_opening = false; /* open, the opening not yet spoken */
    Clock::time_point m_opened_at;
    WatchedText m_status{"menu-status", {"Saved.", "Saved automatically."}};
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

void port_menu_speech_frame(bool open, Clock::time_point now) {
    if (PortMenuReader* reader = s_plugin.reader()) {
        reader->frame(open, now);
    }
}

bool port_menu_speech_step(int direction) {
    PortMenuReader* reader = s_plugin.reader();
    return reader != nullptr && reader->step(direction);
}

}  // namespace a11y
