/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "launcher_speech.hpp"
#include "rmlui_reader.hpp"
#include "speech.hpp"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <string>

namespace a11y {
namespace {

/* The opening waits for focus to land on a control, which it does once the
 * start-up disc check is over. If the page never focuses one, the opening is
 * spoken without it after this long. */
constexpr std::chrono::milliseconds kOpeningFocusWait{1000};

/* Reads one launcher document for as long as it is loaded: the shared page
 * reading, plus Home, the update banner and the Settings view. */
class LauncherReader final : public PageReader {
public:
    LauncherReader(Rml::ElementDocument& document, Speech& speech) : PageReader(document, speech) {}

    void frame(Clock::time_point now) {
        Rml::Element* focus = focused_control();
        if (!m_opened) {
            open(focus, now);
            return;
        }
        const bool settings_shown = displayed("preferences");
        /* Entering Settings: its category's announcement says where the
         * player is, and how to move there. */
        const bool entering = settings_shown && !m_settings_shown;
        follow_focus(focus, entering ? "Settings" : "", entering);
        m_settings_shown = settings_shown;

        SpokenOnce spoken;
        spoken.add(m_status.observe(text_of(m_status.id), now));
        /* Watched while Settings is shown, whichever category: an update check's
         * result still arrives after the player has moved on. */
        for (WatchedText* watch : {&m_settings_status, &m_check_status}) {
            spoken.add(watch->observe_in_view(text_of(watch->id), settings_shown, now));
        }
        /* The banner's own display, not whether Home is the view shown, so
         * coming back from Settings does not announce it again. */
        const bool update_shown = displayed("update-card");
        if (update_shown && !m_update_shown) {
            spoken.add("Update available: " + text_of("update-tag"));
        }
        m_update_shown = update_shown;
        for (WatchedText* watch : {&m_update_title, &m_update_desc}) {
            spoken.add(watch->observe(update_shown ? text_of(watch->id) : std::string(), now));
        }
        for (const std::string& text : spoken.texts()) {
            say(text, Mode::queue);
        }
    }

    /* Up or Down, before the launcher moves focus: in Settings, moves over
     * the lines of the open page between its rows. Returns true when the
     * press was used for that, and focus must stay put. */
    bool step(int direction) {
        if (!m_opened || !displayed("preferences")) {
            stop_reading_lines();
            return false;
        }
        /* The launcher's column of rows (Launcher::column) wraps around. */
        return step_lines(direction, true);
    }

    /* Right, Enter or A on a category whose page had no row to focus. */
    void enter() {
        if (m_opened && displayed("preferences")) {
            enter_text();
        }
    }

    /* Escape, B or Left, before the launcher goes back. */
    bool back() { return m_opened && displayed("preferences") && leave_text(); }

private:
    /* Play is the only way to hear the disc card, which focus never reaches. */
    void add_to_focus_text(const Rml::Element& focus, std::string& text) const override {
        if (focus.GetId() != "play") {
            return;
        }
        std::string disc = text_of("disc-name");
        if (const std::string info = text_of("disc-info"); !info.empty()) {
            disc += ", " + info;
        }
        append_sentence(text, "Disc: " + disc);
    }

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

    bool m_opened = false;
    bool m_update_shown = false;
    bool m_settings_shown = false;
    WatchedText m_status{"status"};
    WatchedText m_settings_status{"settings-status"};
    WatchedText m_check_status{"check-status"};
    WatchedText m_update_title{"update-title"};
    WatchedText m_update_desc{"update-desc"};
};

ReaderPlugin<LauncherReader> s_plugin{"launcher.rml"};

}  // namespace

void launcher_speech_start(Speech& speech) {
    s_plugin.start(speech);
}

void launcher_speech_stop() {
    s_plugin.stop();
}

void launcher_speech_frame(Clock::time_point now) {
    if (LauncherReader* reader = s_plugin.reader()) {
        reader->frame(now);
    }
}

bool launcher_speech_step(int direction) {
    LauncherReader* reader = s_plugin.reader();
    return reader != nullptr && reader->step(direction);
}

void launcher_speech_enter() {
    if (LauncherReader* reader = s_plugin.reader()) {
        reader->enter();
    }
}

bool launcher_speech_back() {
    LauncherReader* reader = s_plugin.reader();
    return reader != nullptr && reader->back();
}

}  // namespace a11y
