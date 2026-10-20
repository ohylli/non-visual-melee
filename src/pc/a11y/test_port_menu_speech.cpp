/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Port menu speech against the real resources/port-menu.rml, loaded into an
 * RmlUi context whose render interface draws nothing. Each test opens, closes
 * and changes the menu through copies of PortMenu's own helpers in
 * src/pc/launcher.cpp, runs the menu's frame hook and checks what reached a
 * fake screen reader bridge. It is the guard against a base merge that
 * reshapes the page. The resources directory is argv[1]. */
#include "port_menu_speech.hpp"
#include "rmlui_reader.hpp"
#include "speech.hpp"
#include "test_rmlui_page.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

using rmlui_test::FakeBridge;
using rmlui_test::interrupt;
using rmlui_test::KeyLog;
using rmlui_test::kFrame;
using rmlui_test::NullRender;
using rmlui_test::OnlineFilter;
using rmlui_test::Output;
using rmlui_test::queue;

std::string s_resources;

std::vector<Output> none() {
    return {};
}

constexpr const char* kOpening = "PC settings. Graphics. Up and down to choose, right to enter";
constexpr const char* kResume = "Resume, button. Close settings and return to the game.";
constexpr const char* kAccessibilityRow =
    "Accessibility, On, Left and right to change. Turns everything Non-Visual Melee adds on or "
    "off: speech, steering and the cues.";
constexpr const char* kSpeechRow =
    "Speech, On, Left and right to change. Turns speech through the screen reader on or off.";

/* The port menu's tab_ids, with the fork's Accessibility and without the
 * launcher's own category. */
constexpr const char* kTabs[] = {
    "tab-graphics", "tab-audio", "tab-cheats", "tab-controls", "tab-online", "tab-accessibility"};
constexpr int kTabCount = 6;
constexpr int kAccessibilityTab = 5;

/* The port menu with port menu speech attached, changed through the same
 * calls PortMenu makes. Loaded hidden, as pc_menu_init loads it when the game
 * starts. */
class Menu {
public:
    explicit Menu(Rml::Context& context, bool speech = true)
        : m_context(context),
          m_speech(a11y::Config{speech, false}, std::make_unique<FakeBridge>(m_outputs)),
          m_describe(*this), m_a11y(m_speech) {
        m_speech.init();
        m_a11y.start(speech);
        a11y::port_menu_speech_start(m_speech);
        m_document = m_context.LoadDocument(s_resources + "/port-menu.rml");
        assert(m_document != nullptr);
        m_document->AddEventListener(Rml::EventId::Textinput, &m_filter, true);
        m_document->AddEventListener(Rml::EventId::Focus, &m_describe, true);
        m_document->AddEventListener(Rml::EventId::Change, &m_change);
        refresh();
        m_context.Update();
    }

    ~Menu() {
        if (m_document != nullptr) {
            unload();
        }
        a11y::port_menu_speech_stop();
        m_speech.shutdown();
    }

    Rml::ElementDocument& document() { return *m_document; }
    a11y::Speech& speech() { return m_speech; }
    rmlui_test::A11ySettings& a11y() { return m_a11y; }

    Rml::Element* element(const char* id) {
        Rml::Element* e = m_document->GetElementById(id);
        assert(e != nullptr);
        return e;
    }

    Rml::Element* focused() { return m_context.GetFocusElement(); }

    /* PortMenu::label */
    void label(const char* id, const std::string& value) {
        Rml::Element* e = element(id);
        e->SetInnerRML("");
        e->AppendChild(m_document->CreateTextNode(value));
    }

    /* PortMenu::slider */
    void slider(const char* id, float value) { element(id)->SetAttribute("value", value); }

    /* refresh_online's write to a text field */
    void field(const char* id, const std::string& value) {
        auto* e = dynamic_cast<Rml::ElementFormControl*>(element(id));
        assert(e != nullptr);
        e->SetValue(value);
    }

    void status(const std::string& value) { label("menu-status", value); }

    /* The parts of PortMenu::refresh the tests hear from, as on a machine
     * with no controller. */
    void refresh() {
        field("net-name", "PLAYER");
        field("net-target", "");
        label("net-delay", "Auto");
        label("net-code", "PLAYER#AB12");
        label("display", "Windowed");
        label("sync", "On");
        label("resolution-val", "Auto");
        label("unlock-all", "Normal");
        label("volume-val", "100%");
        label("port-update-status", "Up to date (v0.1.8-beta)");
        label("pad-port", "Port 1");
        label("pad-name", "No controller in this port.");
        label("bind-name-0", "A");
        label("bind-0", "Unbound");
        label("axis-name-0", "Stick Left");
        label("axis-0", "leftx -");
        label("menu-binding", "Back");
    }

    /* PortMenu::toggle; `quitting` as Quit game sets pc_exit_requested. */
    void toggle(bool quitting = false) {
        m_open = !m_open;
        m_quitting = quitting;
        if (m_open) {
            refresh();
            m_document->Show();
            show_tab(m_tab);
        } else {
            m_document->Hide();
        }
    }

    /* PortMenu::show_tab */
    void show_tab(int next) {
        m_tab = (next + kTabCount) % kTabCount;
        for (int i = 0; i < kTabCount; ++i) {
            element(kTabs[i])->SetClass("selected", i == m_tab);
            const std::string page = std::string("page-") + (kTabs[i] + 4);
            element(page.c_str())->SetClass("selected", i == m_tab);
        }
        focus(kTabs[m_tab]);
    }

    /* PortMenu::enter_rows: Right or Enter on a category. */
    void enter_rows() {
        m_document->UpdateDocument();
        for (const std::string& id : page_focus()) {
            Rml::Element* e = element(id.c_str());
            if (e->IsVisible(true)) {
                focus(id.c_str());
                return;
            }
        }
    }

    /* PortMenu::back: Escape or B. */
    void back() {
        if (focused() != nullptr && !is_sidebar(focused()->GetId())) {
            show_tab(m_tab);
        } else {
            toggle();
        }
    }

    /* PortMenu::focus_step with its hook: Up (-1) or Down (1), in the
     * sidebar over the categories, Resume and Quit game, around; in the rows
     * through PortMenu::show_tab's focus_ids, the category, the page's rows
     * (PortMenu::page_focus), Resume and Quit game. */
    void step(int direction) {
        if (a11y::port_menu_speech_step(direction)) {
            return;
        }
        const Rml::String id = focused() != nullptr ? focused()->GetId() : "";
        if (is_sidebar(id)) {
            const int current = id == "quit-game" ? kTabCount + 1 :
                                id == "resume"    ? kTabCount :
                                                    m_tab;
            const int next = (current + direction + kTabCount + 2) % (kTabCount + 2);
            if (next < kTabCount) {
                show_tab(next);
            } else {
                focus(next == kTabCount ? "resume" : "quit-game");
            }
            return;
        }
        std::vector<std::string> ids{kTabs[m_tab]};
        for (const std::string& row : page_focus()) {
            ids.push_back(row);
        }
        ids.push_back("resume");
        ids.push_back("quit-game");
        const int count = static_cast<int>(ids.size());
        int index = direction > 0 ? -1 : 0;
        for (int i = 0; i < count; ++i) {
            if (m_document->GetElementById(ids[i]) == focused()) {
                index = i;
            }
        }
        focus(ids[(index + direction + count) % count].c_str());
    }

    void focus(const char* id) {
        const bool focused = element(id)->Focus();
        assert(focused);
    }

    /* Left (-1) or Right (1) on one of the fork's rows: PortMenu::step's
     * hand-off, then one frame. */
    void horizontal(int direction) {
        const bool handed = pc_a11y_settings_step(m_document, focused()->GetId(), direction);
        assert(handed);
        frame();
    }

    /* Enter, A or a click on one of the fork's rows: PortMenu::apply's
     * hand-off, then one frame. */
    void press() {
        const bool handed = pc_a11y_settings_press(m_document, focused()->GetId());
        assert(handed);
        frame();
    }

    /* Opens the menu and takes the opening. */
    void open() {
        toggle();
        frame();
        take();
    }

    /* Opens the menu and the rows of a category. */
    void open_rows(int tab) {
        open();
        show_tab(tab);
        enter_rows();
        frame();
        take();
    }

    /* One frame of the game loop: the menu's frame with its hook, then RmlUi
     * updates the page as the next frame draws it. */
    void frame() {
        m_now += kFrame;
        a11y::settings_pages_frame(a11y::SettingsHost::port_menu, m_open);
        a11y::port_menu_speech_frame(m_open, m_quitting, m_now);
        m_context.Update();
    }

    /* Frames until at least `time` has passed. */
    void run(std::chrono::milliseconds time) {
        for (std::chrono::milliseconds t{0}; t < time + kFrame; t += kFrame) {
            frame();
        }
    }

    /* What reached the bridge since the last take. */
    std::vector<Output> take() {
        std::vector<Output> taken = std::move(m_outputs);
        m_outputs.clear();
        return taken;
    }

    /* A key pressed on the focused element, then one frame. */
    void key(Rml::Input::KeyIdentifier key, int modifiers = 0) {
        m_context.ProcessKeyDown(key, modifiers);
        m_context.ProcessKeyUp(key, modifiers);
        frame();
    }

    /* Text typed into the focused element a character a frame; what each
     * frame said. */
    std::vector<Output> type(const std::string& text) {
        for (const char c : text) {
            m_context.ProcessTextInput(Rml::String(1, c));
            frame();
        }
        return take();
    }

    void unload() {
        m_document->RemoveEventListener(Rml::EventId::Textinput, &m_filter, true);
        m_document->RemoveEventListener(Rml::EventId::Focus, &m_describe, true);
        m_document->RemoveEventListener(Rml::EventId::Change, &m_change);
        m_document->Close();
        m_context.Update();
        m_document = nullptr;
    }

private:
    /* PortMenu::ProcessEvent's Focus branch: the status line shows the
     * focused row's description. */
    class Describe final : public Rml::EventListener {
    public:
        explicit Describe(Menu& menu) : m_menu(menu) {}
        void ProcessEvent(Rml::Event& event) override {
            if (!m_menu.m_open) {
                return;
            }
            Rml::String description;
            for (Rml::Element* e = event.GetTargetElement();
                e != nullptr && e != m_menu.m_document && description.empty();
                e = e->GetParentNode())
            {
                description = e->GetAttribute<Rml::String>("data-desc", "");
            }
            if (!description.empty()) {
                m_menu.status(description);
            }
        }

    private:
        Menu& m_menu;
    };

    /* PortMenu::is_sidebar */
    static bool is_sidebar(const Rml::String& id) {
        for (const char* tab : kTabs) {
            if (id == tab) {
                return true;
            }
        }
        return id == "resume" || id == "quit-game";
    }

    /* PortMenu::page_focus */
    std::vector<std::string> page_focus() const {
        switch (m_tab) {
        case 0:
            return {"display", "sync", "resolution", "aspect", "hud-mode", "aa", "filter",
                "filter-mode", "custom-textures", "backend"};
        case 1:
            return {"volume", "music-volume", "sfx-volume", "mute", "reverb", "fps", "scale",
                "port-check-update"};
        case 2:
            return {"unlock-all", "frozen-stadium", "free-camera", "ucf"};
        case 4:
            return {"net-name", "net-target", "net-delay"};
        case 5:
            return pc_a11y_settings_rows();
        default: {
            std::vector<std::string> ids{"pad-port"};
            for (int i = 0; i < 12; ++i) {
                ids.push_back("bind-" + std::to_string(i));
            }
            for (int i = 0; i < 10; ++i) {
                ids.push_back("axis-" + std::to_string(i));
            }
            ids.push_back("bind-reset");
            ids.push_back("menu-binding");
            return ids;
        }
        }
    }

    Rml::Context& m_context;
    OnlineFilter m_filter;
    int m_tab = 0;
    bool m_open = false;
    bool m_quitting = false;
    std::vector<Output> m_outputs;
    a11y::Speech m_speech;
    Describe m_describe;
    rmlui_test::A11ySettings m_a11y;
    rmlui_test::SettingsChange m_change;
    Rml::ElementDocument* m_document = nullptr;
    a11y::Clock::time_point m_now;
};

/* Silent until opened; the opening interrupts, and so does the closing. The
 * status line's description of each focus is never spoken. */
void opening_and_closing(Rml::Context& context) {
    Menu menu(context);
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == none());
    menu.toggle();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kOpening)}));
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == none());
    menu.toggle();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Back to game")}));
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == none());
}

/* The menu opens on the category used last. Escape on a category closes it,
 * Resume too, and Quit game says what it does. */
void reopening_and_the_ways_out(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Audio")}));
    menu.back();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Back to game")}));
    menu.toggle();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "PC settings. Audio. Up and down to choose, right to enter")}));
    /* Up from the first category, around to the sidebar's end. */
    menu.show_tab(0);
    menu.frame();
    menu.take();
    menu.step(-1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Quit game, button. Quit the game and return to the desktop.")}));
    menu.step(-1);
    menu.frame();
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == (std::vector<Output>{interrupt(kResume)}));
    menu.toggle(); /* PortMenu::apply("resume") */
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Back to game")}));
    menu.toggle();
    menu.frame();
    menu.take();
    menu.toggle(true); /* PortMenu::apply("quit-game") */
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Quitting the game")}));
}

/* While closed, the page may change and keep focus, and nothing is said;
 * Up and Down are the menu's. */
void closed_menu_is_silent(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.toggle();
    menu.frame();
    menu.take();
    menu.field("net-target", "ABCD#123"); /* pc_set_net_target */
    menu.status("Canceled.");
    menu.focus("sync");
    menu.run(a11y::kSettleTime * 2);
    assert(!a11y::port_menu_speech_step(1));
    assert(menu.take() == none());
}

/* Every category, then its first row, entered with Right. Descriptions that
 * only repeat the name are left out. */
void every_categorys_first_row(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    const std::vector<std::vector<Output>> expected{
        {interrupt("Display mode, Windowed, Left and right to change")},
        {interrupt("Master volume, slider, 100%. Left and right to adjust. Left/right or drag "
                   "to adjust.")},
        {interrupt("Unlock everything, Normal, Left and right to change. Unlock all 26 "
                   "characters, 11 secret stages, and All-Star mode.")},
        {interrupt("Input device, Port 1, Left and right to change. No controller in this port. "
                   "Choose a controller port or Keyboard. Select a row to change its binding.")},
        {interrupt("Player name, edit, PLAYER. 1–8 letters or digits. Changes apply to the next "
                   "search.")},
        {interrupt(kAccessibilityRow)},
    };
    const char* categories[] = {
        "Graphics", "Audio", "Gameplay", "Controls", "Online", "Accessibility"};
    for (int tab = 0; tab < kTabCount; ++tab) {
        menu.show_tab(tab);
        menu.frame();
        if (tab > 0) {
            assert(menu.take() == (std::vector<Output>{interrupt(categories[tab])}));
        }
        menu.enter_rows();
        menu.frame();
        assert(menu.take() == expected[static_cast<size_t>(tab)]);
        menu.run(a11y::kSettleTime * 2);
        assert(menu.take() == none());
        /* Escape in the rows: back to the category. */
        menu.back();
        menu.frame();
        assert(menu.take() == (std::vector<Output>{interrupt(categories[tab])}));
    }
}

/* Left and Right step a value: the new value alone is said, and "Saved."
 * never. */
void setting_and_its_value(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(0);
    menu.step(1);
    menu.frame();
    assert(menu.take() ==
           (std::vector<Output>{interrupt("Vertical sync, On, Left and right to change")}));
    /* PortMenu::apply("sync"), then saved() and its refresh. */
    menu.label("sync", "Off");
    menu.status("Saved.");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Off")}));
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == none());
    /* A badge is part of the name. */
    menu.label("aa", "4x MSAA");
    menu.focus("aa");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Anti-aliasing RESTART, 4x MSAA, Left and right to change")}));
    /* A row's own description, once the name is left out of it. */
    menu.focus("aspect");
    menu.frame();
    assert(menu.take() ==
           (std::vector<Output>{interrupt(
               "Aspect ratio, Original (73:60), Left and right to change. Applies to fights. Menus "
               "and cutscenes keep the original aspect. Applied when the menu closes.")}));
}

/* Left and Right step a slider; Enter cycles it, wrapping from the top. */
void slider_steps_and_wraps(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(1);
    /* PortMenu::step(-1), then saved(). */
    menu.slider("volume", 95.0f);
    menu.label("volume-val", "95%");
    menu.status("Saved.");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("95%")}));
    /* PortMenu::apply("volume") at the top. */
    menu.slider("volume", 100.0f);
    menu.label("volume-val", "100%");
    menu.frame();
    menu.slider("volume", 0.0f);
    menu.label("volume-val", "0%");
    menu.frame();
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == (std::vector<Output>{interrupt("100%"), interrupt("0%")}));
    /* Each slider has its own name. */
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Music volume, slider, 100%. Left and right to adjust. Background "
                              "music volume.")}));
}

/* An Online field echoes typing and explains a refused character. */
void text_field_typing(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(4);
    menu.key(Rml::Input::KI_END);
    menu.take();
    assert(menu.type("#") == (std::vector<Output>{interrupt("# not allowed")}));
    menu.key(Rml::Input::KI_BACK);
    assert(menu.take() == (std::vector<Output>{interrupt("PLAYE")}));
    assert(menu.type("r12") ==
           (std::vector<Output>{interrupt("PLAYER"), interrupt("PLAYER1"), interrupt("PLAYER12")}));
    assert(menu.type("3") == (std::vector<Output>{interrupt("Full, 8 characters at most")}));
    /* The connect code takes #. */
    menu.focus("net-target");
    menu.frame();
    menu.take();
    assert(menu.type("a#") == (std::vector<Output>{interrupt("A"), interrupt("A#")}));
}

/* Controls: the input device's live line, the bindings, a heading between
 * the buttons and axes, and the keyboard's lines after the last row, before
 * Resume. */
void controls_rows_and_lines(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(3);
    /* PortMenu::apply("pad-port", -1): around to Keyboard, whose live line
     * comes with it. */
    menu.label("pad-port", "Keyboard");
    menu.label("pad-name", "Player 1 · Escape cancels · Delete clears a binding");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Keyboard, Player 1, Escape cancels, Delete clears a binding")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("A, Unbound, Enter to change")}));

    menu.focus("bind-11");
    menu.frame();
    menu.take();
    menu.step(1);
    assert(menu.take() == (std::vector<Output>{interrupt("Sticks and triggers")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Stick Left, leftx -, Enter to change")}));
    assert(menu.focused() == menu.element("axis-0"));

    menu.focus("bind-reset");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Restore defaults: Restore, button. Put this port back on its "
                              "default mapping.")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Menu button, Back, Enter to change. Choose the button that opens "
                              "settings on all controllers. Guide and F1 always work too.")}));
    const std::vector<std::string> lines{"Keyboard", "WASD, arrows Move", "X Attack", "Z Special",
        "C, V Jump", "Q, E Shield", "Tab Grab", "Enter Start", "IJKL C-stick"};
    for (const std::string& line : lines) {
        menu.step(1);
        menu.frame();
        assert(menu.take() == (std::vector<Output>{interrupt(line)}));
    }
    assert(menu.focused() == menu.element("menu-binding"));
    menu.step(-1);
    assert(menu.take() == (std::vector<Output>{interrupt(lines[lines.size() - 2])}));
    menu.step(1);
    menu.step(1);
    menu.frame();
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == (std::vector<Output>{interrupt(lines.back()), interrupt(kResume)}));
    assert(menu.focused() == menu.element("resume"));
    /* Up from Resume is the sidebar's: the last category, not the lines. */
    menu.step(-1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Accessibility")}));
}

/* Online: the connect code between the fields, the closing note after Input
 * delay, then Resume. */
void online_lines(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(4);
    menu.step(1);
    assert(menu.take() == (std::vector<Output>{interrupt("Your connect code, PLAYER#AB12")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Friend's connect code, edit, blank. Leave blank to host your code. "
                              "Open Online, Direct Connect in-game.")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Input delay, Auto, Left and right to change. Automatic adapts to "
                              "your connection. Changes apply next match.")}));
    menu.step(1);
    assert(menu.take() ==
           (std::vector<Output>{interrupt("Internet discovery can take a few minutes. Some home "
                                          "networks cannot connect without a relay.")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kResume)}));
    /* Up from the first row onto the category, which has nothing above. */
    menu.show_tab(4);
    menu.enter_rows();
    menu.frame();
    menu.take();
    menu.step(-1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Online")}));
}

/* The status line is spoken queued once it settles, except "Saved.", which
 * follows every change, the description it shows for each focus, and what it
 * shows as the menu opens. */
void status_line(Rml::Context& context) {
    Menu menu(context);
    menu.status("Saved.");
    menu.open();
    menu.status("Defaults restored.");
    menu.run(a11y::kSettleTime);
    assert(menu.take() == (std::vector<Output>{queue("Defaults restored.")}));
    menu.status("Saved.");
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == none());
    menu.status("Press a button on the controller. Escape cancels.");
    menu.run(a11y::kSettleTime);
    menu.status("Canceled.");
    menu.run(a11y::kSettleTime);
    assert(menu.take() ==
           (std::vector<Output>{
               queue("Press a button on the controller. Escape cancels."), queue("Canceled.")}));
    /* A move writes the new focus's description: said with the focus only. */
    menu.enter_rows();
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() ==
           (std::vector<Output>{interrupt("Display mode, Windowed, Left and right to change")}));
    /* Closed and opened again on the same text: only the opening. */
    menu.toggle();
    menu.frame();
    menu.take();
    menu.status("Settings save automatically.");
    menu.frame();
    menu.toggle();
    menu.frame();
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == (std::vector<Output>{interrupt(kOpening)}));
}

/* PortMenu::apply("port-check-update") writes the same text to both regions;
 * the result shows when the next save refreshes the page. The row's
 * description is left out: it is the result the page showed first. */
void update_check(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(1);
    menu.focus("port-check-update");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Check for updates: Check now, button. Up to date (v0.1.8-beta)")}));
    menu.label("port-update-status", "Checking for updates...");
    menu.status("Checking for updates...");
    menu.run(a11y::kSettleTime);
    assert(menu.take() == (std::vector<Output>{queue("Checking for updates...")}));
    /* The result arrives after the player has moved to another category. */
    menu.show_tab(0);
    menu.frame();
    menu.take();
    menu.label("port-update-status", "Update available: v0.2.0");
    menu.status("Saved.");
    menu.run(a11y::kSettleTime);
    assert(menu.take() == (std::vector<Output>{queue("Update available: v0.2.0")}));
}

void tab_becomes_down(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    KeyLog log;
    menu.document().AddEventListener(Rml::EventId::Keydown, &log);
    context.ProcessKeyDown(Rml::Input::KI_TAB, 0);
    context.ProcessKeyDown(Rml::Input::KI_TAB, Rml::Input::KM_SHIFT);
    menu.document().RemoveEventListener(Rml::EventId::Keydown, &log);
    assert(log.keys == (std::vector<int>{Rml::Input::KI_DOWN, Rml::Input::KI_UP}));
    /* RmlUi's own Tab navigation did not run. */
    assert(menu.focused() == menu.element("tab-graphics"));
}

/* With the accessibility switch off, Up and Down move focus as in the base
 * port, over the lines. */
void speech_off_leaves_the_keys_alone(Rml::Context& context) {
    Menu menu(context, false);
    menu.open_rows(4);
    menu.step(1);
    assert(menu.focused() == menu.element("net-target"));
}

/* Speech switched off and on again, as the switches do it (switches.hpp):
 * what the page did meanwhile goes unsaid, and once speech is back the
 * focused row is announced again, queued behind the switch's own words. */
void speech_back_on_announces_the_focused_row(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(0);
    menu.step(1);
    menu.frame();
    menu.take();
    menu.speech().announce("Speech off", a11y::Mode::interrupt);
    menu.speech().set_on(false);
    menu.label("sync", "Off");
    menu.frame();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Speech off")}));
    menu.speech().set_on(true);
    menu.speech().announce("Speech on", a11y::Mode::interrupt);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Speech on"),
                              queue("Vertical sync, Off, Left and right to change")}));
    menu.frame();
    assert(menu.take() == none());
    /* Back on while the menu is closed: its next opening says where it is,
     * once. */
    menu.toggle();
    menu.frame();
    menu.take();
    menu.speech().set_on(false);
    menu.frame();
    menu.speech().set_on(true);
    menu.frame();
    menu.toggle();
    menu.frame();
    assert(menu.take().size() == 1);
    menu.frame();
    assert(menu.take() == none());
}

/* The fork's category, last before Resume: its rows read as the menu's own,
 * the description under each name left out; the column of rows ends at
 * Resume. */
void accessibility_category_rows(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.show_tab(kAccessibilityTab);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Accessibility")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kResume)}));
    menu.show_tab(kAccessibilityTab);
    menu.enter_rows();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kAccessibilityRow)}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kSpeechRow)}));
    menu.focus("a11y-ledge_warning-distance");
    menu.frame();
    assert(menu.take() ==
           (std::vector<Output>{interrupt(
               "Ledge warning distance, slider, 40 units. Left and right to adjust. How far from "
               "the edge the warning starts, in stage units.")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kResume)}));
}

/* A switch flips with Left, Right, Enter or A and says its new value; the
 * menu's confirm sound plays, as for its own settings, and "Saved." stays
 * unsaid. */
void accessibility_switch(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(kAccessibilityTab);
    menu.focus("a11y-gap_warning-on");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Gap warning, On, Left and right to change. Turns the gap warning "
                              "on or off.")}));
    menu.a11y().take();
    menu.horizontal(-1);
    assert(menu.take() == (std::vector<Output>{interrupt("Off")}));
    assert(menu.a11y().value("gap_warning.on").value == 0.0f);
    assert(menu.a11y().take() == (std::vector<std::string>{"sound 1"}));
    menu.press();
    assert(menu.take() == (std::vector<Output>{interrupt("On")}));
    assert(menu.a11y().take() == (std::vector<std::string>{"sound 1"}));
    menu.status("Saved.");
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == none());
}

/* A slider steps by its value's step with the move sound, by the D-pad
 * through the hand-off and by the keyboard's arrows through RmlUi's slider;
 * Enter or A leaves it as it is, silently. */
void accessibility_slider(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(kAccessibilityTab);
    menu.focus("a11y-cues-volume");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Cue volume, slider, 100%. Left and right to adjust. The loudness of "
                              "all cues together, under the master volume; the game's sound "
                              "effect and music volumes leave cues alone.")}));
    menu.a11y().take();
    menu.horizontal(-1);
    assert(menu.take() == (std::vector<Output>{interrupt("95%")}));
    assert(menu.a11y().take() == (std::vector<std::string>{"sound 2"}));
    menu.key(Rml::Input::KI_LEFT);
    assert(menu.take() == (std::vector<Output>{interrupt("90%")}));
    assert(menu.a11y().value("cues.volume").value == 90.0f);
    assert(menu.a11y().take() == (std::vector<std::string>{"sound 2"}));
    menu.press();
    assert(menu.take() == none());
    assert(menu.a11y().take().empty());
}

/* Preview plays its cue, silently and with no sound of the menu's; leaving
 * the row stops it, and so does closing the menu. */
void accessibility_preview(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(kAccessibilityTab);
    menu.focus("a11y-ledge_warning-preview");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Preview ledge warning: Play, button. Plays a short sample of the "
                              "ledge warning, as it sounds in a match.")}));
    menu.a11y().take();
    menu.press();
    assert(menu.take() == none());
    assert(menu.a11y().take() == (std::vector<std::string>{"preview ledge_warning"}));
    menu.step(-1);
    menu.frame();
    assert(menu.a11y().take() == (std::vector<std::string>{"stop"}));
    menu.step(1);
    menu.press();
    menu.take();
    assert(menu.a11y().take() == (std::vector<std::string>{"preview ledge_warning"}));
    menu.toggle();
    menu.frame();
    assert(menu.a11y().take() == (std::vector<std::string>{"stop"}));
    assert(menu.take() == (std::vector<Output>{interrupt("Back to game")}));
}

/* Speech switched off from its row says so last; on again, it says so
 * first and the row announces itself again. */
void accessibility_switches_speech(Rml::Context& context) {
    Menu menu(context);
    menu.open_rows(kAccessibilityTab);
    menu.step(1);
    menu.frame();
    menu.take();
    menu.press();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Speech off")}));
    /* The menu's keys are its own meanwhile. */
    assert(!a11y::port_menu_speech_step(1));
    menu.press();
    assert(menu.take() == (std::vector<Output>{interrupt("Speech on"), queue(kSpeechRow)}));
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == none());
}

/* Every control in the menu, on every page, has a name. Guards against a
 * base merge adding a row the reader cannot name. */
void every_control_has_a_name(Rml::Context& context) {
    Menu menu(context);
    int controls = 0;
    for (const char* tag : {"button", "input", "select", "textarea"}) {
        Rml::ElementList elements;
        menu.document().GetElementsByTagName(elements, tag);
        for (const Rml::Element* element : elements) {
            const a11y::Control control = a11y::describe_control(*element);
            if (control.name.empty() || control.role == a11y::Role::other) {
                std::cerr << "unnamed control: #" << element->GetId() << '\n';
            }
            assert(!control.name.empty());
            assert(control.role != a11y::Role::other);
            ++controls;
        }
    }
    assert(controls > 40); /* the walk found the page */
}

/* The launcher's document is the launcher reader's. */
void other_documents_are_ignored(Rml::Context& context) {
    std::vector<Output> outputs;
    a11y::Speech speech(a11y::Config{true, false}, std::make_unique<FakeBridge>(outputs));
    speech.init();
    a11y::port_menu_speech_start(speech);
    Rml::ElementDocument* document = context.LoadDocument(s_resources + "/launcher.rml");
    assert(document != nullptr);
    document->Show();
    a11y::Clock::time_point now;
    for (int i = 0; i < 60; ++i) {
        now += kFrame;
        context.Update();
        a11y::port_menu_speech_frame(true, false, now);
    }
    assert(!a11y::port_menu_speech_step(1));
    document->Close();
    context.Update();
    a11y::port_menu_speech_stop();
    speech.shutdown();
    assert(outputs.empty());
}

void unloading_stops_the_reader(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.unload();
    menu.run(a11y::kSettleTime);
    assert(menu.take() == none());
}

}  // namespace

/* Speech's log goes nowhere here; each Menu turns it off anyway. */
extern "C" void pc_log_line(const char*, ...) {}

int main(int argc, char** argv) {
    assert(argc == 2);
    s_resources = argv[1];
    NullRender render;
    Rml::SetRenderInterface(&render);
    Rml::Initialise();
    rmlui_test::load_fonts(s_resources);
    Rml::Context* context = Rml::CreateContext("main", Rml::Vector2i(1280, 720));
    assert(context != nullptr);

    opening_and_closing(*context);
    reopening_and_the_ways_out(*context);
    closed_menu_is_silent(*context);
    every_categorys_first_row(*context);
    setting_and_its_value(*context);
    slider_steps_and_wraps(*context);
    text_field_typing(*context);
    controls_rows_and_lines(*context);
    online_lines(*context);
    status_line(*context);
    update_check(*context);
    tab_becomes_down(*context);
    speech_off_leaves_the_keys_alone(*context);
    speech_back_on_announces_the_focused_row(*context);
    accessibility_category_rows(*context);
    accessibility_switch(*context);
    accessibility_slider(*context);
    accessibility_preview(*context);
    accessibility_switches_speech(*context);
    every_control_has_a_name(*context);
    other_documents_are_ignored(*context);
    unloading_stops_the_reader(*context);

    Rml::Shutdown();
    std::cout << "port_menu_speech: all tests passed\n";
    return 0;
}
