/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Launcher speech against the real resources/launcher.rml, loaded into an
 * RmlUi context whose render interface draws nothing. Each test changes the
 * page the way src/pc/launcher.cpp does, runs the reader's frame step and
 * checks what reached a fake screen reader bridge. It is the guard against a
 * base merge that reshapes the page. The resources directory is argv[1]. */
#include "launcher_speech.hpp"
#include "rmlui_reader.hpp"
#include "speech.hpp"
#include "test_rmlui_page.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <cassert>
#include <cstdarg>
#include <cstdio>
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

/* One launcher page with launcher speech attached, changed through the same
 * calls launcher.cpp makes. */
class Page {
public:
    explicit Page(Rml::Context& context, const char* file = "launcher.rml", bool speech = true)
        : m_context(context),
          m_speech(a11y::Config{speech, false}, std::make_unique<FakeBridge>(m_outputs)) {
        m_speech.init();
        a11y::launcher_speech_start(m_speech);
        m_document = m_context.LoadDocument(s_resources + "/" + file);
        assert(m_document != nullptr);
        m_document->AddEventListener(Rml::EventId::Textinput, &m_filter, true);
        m_document->Show();
    }

    ~Page() {
        if (m_document != nullptr) {
            close();
        }
        a11y::launcher_speech_stop();
        m_speech.shutdown();
    }

    Rml::ElementDocument& document() { return *m_document; }

    Rml::Element* element(const char* id) {
        Rml::Element* e = m_document->GetElementById(id);
        assert(e != nullptr);
        return e;
    }

    /* Launcher::text */
    void text(const char* id, const std::string& value) {
        Rml::Element* e = element(id);
        e->SetInnerRML("");
        e->AppendChild(m_document->CreateTextNode(value));
    }

    /* Launcher::enabled */
    void enabled(const char* id, bool value) {
        Rml::Element* e = element(id);
        e->SetPseudoClass("disabled", !value);
        e->SetProperty("focus", value ? "auto" : "none");
        e->SetProperty("tab-index", value ? "auto" : "none");
        if (!value) {
            e->Blur();
        } else {
            m_document->UpdateDocument();
        }
    }

    void status(const std::string& value) { text("status", value); }

    /* refresh_online's write to a text field */
    void field(const char* id, const std::string& value) {
        auto* e = dynamic_cast<Rml::ElementFormControl*>(element(id));
        assert(e != nullptr);
        e->SetValue(value);
    }

    /* Launcher::show_tab */
    void show_tab(int tab) {
        static constexpr const char* pages[] = {
            "page-graphics", "page-audio", "page-cheats", "page-controls", "page-online"};
        for (int i = 0; i < 5; ++i) {
            element(kTabs[i])->SetClass("selected", i == tab);
            element(pages[i])->SetClass("selected", i == tab);
        }
        m_tab = tab;
        focus(kTabs[tab]);
    }

    /* Launcher::focus_step on the Controls or Online tab, with its hook: Up
     * (-1) or Down (1) through the tab, the page's controls
     * (Launcher::page_focus) and Back. */
    void step(int direction) {
        if (a11y::launcher_speech_step(direction)) {
            return;
        }
        std::vector<const char*> ids{kTabs[m_tab]};
        if (m_tab == 4) {
            ids.insert(ids.end(), {"net-name", "net-target", "net-delay"});
        }
        ids.push_back("back");
        const int count = static_cast<int>(ids.size());
        int index = direction > 0 ? -1 : 0;
        for (int i = 0; i < count; ++i) {
            if (element(ids[i]) == m_context.GetFocusElement()) {
                index = i;
            }
        }
        focus(ids[(index + direction + count) % count]);
    }

    /* Launcher::show_settings, with the parts of refresh_settings the tests
     * hear from. */
    void show_settings(bool show) {
        element("home")->SetProperty("display", show ? "none" : "block");
        element("preferences")->SetProperty("display", show ? "block" : "none");
        if (!show) {
            focus("settings");
            return;
        }
        text("sync", "On");
        text("aa", "4x MSAA");
        text("volume-val", "100%");
        field("net-name", "OTTO");
        text("net-code", "ABCD#123");
        text("check-status", "Melee-PC is up to date (v0.1.8-beta).");
        text("settings-status", "Saved automatically.");
        show_tab(0);
    }

    /* Opens with a disc, then enters Settings. */
    void open_settings() {
        open_with_disc();
        show_settings(true);
        frame();
        take();
    }

    void focus(const char* id) {
        const bool focused = element(id)->Focus();
        assert(focused);
    }

    /* The launcher's constructor with no disc remembered, then the opening. */
    void open_without_disc() {
        enabled("play", false);
        enabled("verify", false);
        focus("choose");
        run(a11y::kSettleTime);
        assert(take() == (std::vector<Output>{
                             queue("Melee launcher. No disc selected. Choose disc, button")}));
    }

    /* The launcher's constructor and start-up disc check with a good disc
     * remembered, then the opening. */
    void open_with_disc() {
        enabled("play", false);
        enabled("verify", false);
        focus("choose");
        status("Checking disc image...");
        for (const char* id : {"choose", "settings", "discord"}) {
            enabled(id, false);
        }
        run(a11y::kSettleTime * 2);
        assert(take().empty());
        text("disc-name", "melee.iso");
        text("disc-path", "C:/games/melee.iso");
        text("disc-info", "Super Smash Bros. Melee / USA / Revision 2 (1.02)");
        status("Ready to play / Disc not verified.");
        for (const char* id : {"play", "choose", "verify", "settings", "discord"}) {
            enabled(id, true);
        }
        focus("play");
        run(a11y::kSettleTime);
        assert(take() == (std::vector<Output>{queue(
                             "Melee launcher. Ready to play, Disc not verified. Play Melee, "
                             "button. Disc: melee.iso, Super Smash Bros. Melee, USA, Revision 2 "
                             "(1.02)")}));
    }

    /* One turn of the launcher's loop: RmlUi updates the page, then the hook. */
    void frame() {
        m_now += kFrame;
        m_context.Update();
        a11y::launcher_speech_frame(m_now);
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

    /* A key pressed on the focused element, then one turn of the loop. */
    void key(Rml::Input::KeyIdentifier key, int modifiers = 0) {
        m_context.ProcessKeyDown(key, modifiers);
        m_context.ProcessKeyUp(key, modifiers);
        frame();
    }

    /* Enter as aurora passes it on: the key, then a line break as typed
     * text, then one turn of the loop. */
    void enter() {
        m_context.ProcessKeyDown(Rml::Input::KI_RETURN, 0);
        m_context.ProcessTextInput('\n');
        m_context.ProcessKeyUp(Rml::Input::KI_RETURN, 0);
        frame();
    }

    /* Up (-1) or Down (1) as the player presses it: the focused element's
     * own handling, then the launcher's step, then one turn of the loop. */
    void arrow(int direction) {
        const auto arrow = direction > 0 ? Rml::Input::KI_DOWN : Rml::Input::KI_UP;
        m_context.ProcessKeyDown(arrow, 0);
        step(direction);
        m_context.ProcessKeyUp(arrow, 0);
        frame();
    }

    /* Text typed into the focused element a character a frame, as a player
     * types; what each frame said. */
    std::vector<Output> type(const std::string& text) {
        for (const char c : text) {
            m_context.ProcessTextInput(Rml::String(1, c));
            frame();
        }
        return take();
    }

    void close() {
        m_document->RemoveEventListener(Rml::EventId::Textinput, &m_filter, true);
        m_document->Close();
        m_context.Update();
        m_document = nullptr;
    }

private:
    static constexpr const char* kTabs[] = {
        "tab-graphics", "tab-audio", "tab-cheats", "tab-controls", "tab-online"};

    Rml::Context& m_context;
    OnlineFilter m_filter;
    int m_tab = 0;
    std::vector<Output> m_outputs;
    a11y::Speech m_speech;
    Rml::ElementDocument* m_document = nullptr;
    a11y::Clock::time_point m_now;
};

void opening_without_disc(Rml::Context& context) {
    Page page(context);
    page.enabled("play", false);
    page.enabled("verify", false);
    page.focus("choose");
    page.run(a11y::kSettleTime - kFrame * 2);
    assert(page.take().empty());
    page.run(kFrame * 2);
    assert(page.take() ==
           (std::vector<Output>{queue("Melee launcher. No disc selected. Choose disc, button")}));
}

/* Also the Play announcement, which carries the disc. */
void opening_waits_for_the_disc_check(Rml::Context& context) {
    Page page(context);
    page.open_with_disc();
}

void focus_change_is_announced(Rml::Context& context) {
    Page page(context);
    page.open_with_disc();
    page.focus("choose");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Choose disc, button")}));
    page.focus("discord");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Discord, button")}));
}

/* The file dialog disables Choose disc, which parks focus on its container,
 * and focuses it again when cancelled. */
void returning_to_the_same_control_is_silent(Rml::Context& context) {
    Page page(context);
    page.open_without_disc();
    page.enabled("choose", false);
    page.frame();
    page.enabled("choose", true);
    page.focus("choose");
    page.run(a11y::kSettleTime);
    assert(page.take().empty());
}

void status_is_spoken_once_settled(Rml::Context& context) {
    Page page(context);
    page.open_without_disc();
    page.status("Opening Discord in browser...");
    page.run(a11y::kSettleTime - kFrame * 4);
    page.status("Could not open the browser.");
    page.run(a11y::kSettleTime - kFrame);
    assert(page.take().empty());
    page.run(kFrame);
    assert(page.take() == (std::vector<Output>{queue("Could not open the browser.")}));
}

void verify_reports_progress_in_steps(Rml::Context& context) {
    Page page(context);
    page.open_with_disc();
    page.focus("verify");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Verify disc, button")}));

    /* Launcher::action("verify"): busy, so everything else is disabled. */
    for (const char* id : {"play", "choose", "settings", "discord"}) {
        page.enabled(id, false);
    }
    page.text("verify", "Cancel verification");
    page.focus("verify");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Cancel verification")}));

    for (int percent = 0; percent <= 100; percent += 3) {
        page.status("Verifying disc / " + std::to_string(percent) + "%");
        page.frame();
    }
    page.status("Verifying disc / 100%");
    page.frame();
    assert(page.take() == (std::vector<Output>{queue("Verifying disc, 21%"),
                              queue("Verifying disc, 42%"), queue("Verifying disc, 60%"),
                              queue("Verifying disc, 81%"), queue("Verifying disc, 100%")}));

    page.status("Verified / matches the original USA revision 2 disc.");
    for (const char* id : {"play", "choose", "settings", "discord"}) {
        page.enabled(id, true);
    }
    page.text("verify", "Verify disc");
    page.focus("verify");
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{interrupt("Verify disc"),
                              queue("Verified, matches the original USA revision 2 disc.")}));
}

void update_banner_is_announced(Rml::Context& context) {
    Page page(context);
    page.open_without_disc();
    page.element("update-card")->SetProperty("display", "flex");
    page.text("update-tag", "v0.2.0");
    page.text("update-title", "Melee PC v0.2.0");
    page.text("update-desc", "A newer version of Melee PC is available (v0.2.0).");
    page.run(a11y::kSettleTime);
    assert(page.take() ==
           (std::vector<Output>{queue("Update available: v0.2.0"), queue("Melee PC v0.2.0"),
               queue("A newer version of Melee PC is available (v0.2.0).")}));

    /* Launcher::show_settings there and back: the banner stays put. */
    page.element("home")->SetProperty("display", "none");
    page.element("preferences")->SetProperty("display", "block");
    page.run(a11y::kSettleTime);
    page.element("home")->SetProperty("display", "block");
    page.element("preferences")->SetProperty("display", "none");
    page.run(a11y::kSettleTime);
    assert(page.take().empty());

    page.focus("update-action");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Update Now, button")}));
    page.text("update-action", "Downloading...");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Downloading...")}));
}

void tab_becomes_down(Rml::Context& context) {
    Page page(context);
    page.open_without_disc();
    KeyLog log;
    page.document().AddEventListener(Rml::EventId::Keydown, &log);
    context.ProcessKeyDown(Rml::Input::KI_TAB, 0);
    context.ProcessKeyDown(Rml::Input::KI_TAB, Rml::Input::KM_SHIFT);
    page.document().RemoveEventListener(Rml::EventId::Keydown, &log);
    assert(log.keys == (std::vector<int>{Rml::Input::KI_DOWN, Rml::Input::KI_UP}));
    /* RmlUi's own Tab navigation did not run. */
    assert(context.GetFocusElement() == page.element("choose"));
}

void entering_and_leaving_settings(Rml::Context& context) {
    Page page(context);
    page.open_with_disc();
    page.focus("settings");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Settings, button")}));
    page.show_settings(true);
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Settings. Graphics, tab, selected, 1 of 5. Left and right to switch "
                              "tabs")}));
    page.show_settings(false);
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{interrupt("Settings, button")}));
}

/* A tab whose page opens with rows without a control says so; Up and Down
 * read them. A page that opens with a control does not. */
void tab_with_text_gives_a_hint(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(4);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Online, tab, selected, 5 of 5. Left and right to switch tabs")}));
    page.show_tab(3);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Controls, tab, selected, 4 of 5. Left and right to switch tabs. "
                              "Text below, Down to read")}));
}

/* Headings, line breaks and " · " separators each end a line. */
void controls_page_line_by_line(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(3);
    page.frame();
    page.take();
    const std::vector<std::string> lines{"Keyboard", "WASD, arrows Move", "X Attack", "Z Special",
        "C, V Jump", "Q, E Shield", "Tab Grab", "Enter Start", "IJKL C-stick", "Controller",
        "Remap a gamepad from the in-game menu: press BACK on the pad, or F1, then open "
        "Controls. Controllers are only detected once the game is running."};
    for (const std::string& line : lines) {
        page.step(1);
        page.frame();
        assert(page.take() == (std::vector<Output>{interrupt(line)}));
    }
    assert(page.document().GetContext()->GetFocusElement() == page.element("tab-controls"));
    page.step(-1);
    assert(page.take() == (std::vector<Output>{interrupt("Controller")}));
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt(lines.back())}));

    /* Off the last line onto Back, and from Back up onto the last line. */
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Back to launcher, button")}));
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(lines.back())}));
    assert(page.document().GetContext()->GetFocusElement() == page.element("back"));
    /* Back down to Back, where focus stayed. */
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Back to launcher, button")}));

    /* Down from Back wraps to the tab; up from the first line returns to it. */
    page.step(1);
    page.frame();
    page.take();
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt("Keyboard")}));
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Controls, tab, selected, 4 of 5. Left and right to switch tabs. "
                              "Text below, Down to read")}));
}

/* The lines sit between the controls, in the page's order. */
void online_page_lines_between_controls(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(4);
    page.frame();
    page.step(1);
    page.frame();
    page.take();
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt("Your connect code")}));
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt("ABCD#123")}));
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Friend's connect code, edit, blank. Leave blank to host your code. "
                              "Open Online, Direct Connect in-game.")}));
    page.step(-1);
    assert(page.take() == (std::vector<Output>{interrupt("ABCD#123")}));
    page.step(-1);
    page.step(-1);
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Your connect code"),
               interrupt("Player name, edit, OTTO. 1–8 letters or digits. Changes apply to the "
                         "next search.")}));

    page.focus("net-delay");
    page.frame();
    page.take();
    page.step(1);
    assert(page.take() ==
           (std::vector<Output>{interrupt("Internet discovery can take a few minutes. Some home "
                                          "networks cannot connect without a relay.")}));
    /* A value change on the control focus is on ends the reading; the next
     * press starts from that control again. */
    page.text("net-delay", "1 frame");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("1 frame")}));
    page.step(1);
    assert(page.take() ==
           (std::vector<Output>{interrupt("Internet discovery can take a few minutes. Some home "
                                          "networks cannot connect without a relay.")}));

    /* A line rewritten while it is held: the next press starts from the
     * control again. */
    page.focus("net-name");
    page.frame();
    page.take();
    page.step(1);
    page.step(1);
    page.take();
    page.text("net-code", "WXYZ#999");
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt("Your connect code")}));
    page.text("net-code", "ABCD#123");
}

/* Switching tabs while on a line: the new tab starts from its top. */
void tab_switch_ends_reading(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(3);
    page.frame();
    page.step(1);
    page.step(1);
    page.take();
    page.show_tab(4);
    page.frame();
    page.take();
    page.step(1);
    page.frame();
    assert(page.document().GetContext()->GetFocusElement() == page.element("net-name"));
    assert(page.take() ==
           (std::vector<Output>{interrupt("Player name, edit, OTTO. 1–8 letters or digits. "
                                          "Changes apply to the next search.")}));
}

/* With the accessibility switch off, Up and Down move focus as in the base
 * port. */
void speech_off_leaves_the_keys_alone(Rml::Context& context) {
    Page page(context, "launcher.rml", false);
    page.enabled("play", false);
    page.enabled("verify", false);
    page.focus("choose");
    page.run(a11y::kSettleTime * 2);
    page.show_settings(true);
    page.show_tab(3);
    page.frame();
    page.step(1);
    assert(page.document().GetContext()->GetFocusElement() == page.element("back"));
}

void cycling_setting_and_its_value(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.focus("sync");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Vertical sync, On, Enter to change. Synchronise "
                                          "presentation with your display.")}));
    page.text("sync", "Off");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Off")}));
    /* A badge is part of the name. */
    page.focus("aa");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Anti-aliasing RESTART, 4x MSAA, Enter to change. "
                                          "Smooth polygon edges with multisampling.")}));
}

/* Launcher::step_slider writes the slider and the span beside it. */
void slider_step(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(1);
    page.focus("volume");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Master volume, slider, 100%. Left and right to adjust. "
                                          "Use left/right arrows or drag slider.")}));
    page.element("volume")->SetAttribute("value", 95.0f);
    page.text("volume-val", "95%");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("95%")}));
}

/* Focuses the Player name field, holding OTTO, with the cursor at its end. */
void focus_player_name(Page& page) {
    page.open_settings();
    page.show_tab(4);
    page.focus("net-name");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Player name, edit, OTTO. 1–8 letters or digits. "
                                          "Changes apply to the next search.")}));
    page.key(Rml::Input::KI_END);
    page.take();
}

/* Each change speaks the whole value; a refused character and a full field
 * say why nothing changed. */
void text_field_typing(Rml::Context& context) {
    Page page(context);
    focus_player_name(page);
    page.key(Rml::Input::KI_BACK);
    assert(page.take() == (std::vector<Output>{interrupt("OTT")}));
    page.key(Rml::Input::KI_BACK);
    assert(page.take() == (std::vector<Output>{interrupt("OT")}));
    /* Letters go in as capitals. */
    assert(page.type("t") == (std::vector<Output>{interrupt("OTT")}));
    assert(page.type("#") == (std::vector<Output>{interrupt("# not allowed")}));
    assert(page.type(" ") == (std::vector<Output>{interrupt("space not allowed")}));
    /* Enter types a line break, which no field takes: nothing to say. */
    page.enter();
    assert(page.take().empty());
    assert(page.type("o1234") ==
           (std::vector<Output>{interrupt("OTTO"), interrupt("OTTO1"), interrupt("OTTO12"),
               interrupt("OTTO123"), interrupt("OTTO1234")}));
    assert(page.type("5") == (std::vector<Output>{interrupt("Full, 8 characters at most")}));
    page.enter();
    assert(page.take().empty());
    /* The connect code takes #. */
    page.focus("net-target");
    page.frame();
    page.take();
    assert(page.type("ab#") ==
           (std::vector<Output>{interrupt("A"), interrupt("AB"), interrupt("AB#")}));
    for (int i = 0; i < 3; ++i) {
        page.key(Rml::Input::KI_BACK);
    }
    assert(
        page.take() == (std::vector<Output>{interrupt("AB"), interrupt("A"), interrupt("blank")}));
}

/* A cursor move says the character Delete would remove, or blank at the end. */
void text_field_cursor(Rml::Context& context) {
    Page page(context);
    focus_player_name(page);
    page.key(Rml::Input::KI_LEFT);
    assert(page.take() == (std::vector<Output>{interrupt("O")}));
    page.key(Rml::Input::KI_LEFT);
    assert(page.take() == (std::vector<Output>{interrupt("T")}));
    page.key(Rml::Input::KI_HOME);
    assert(page.take() == (std::vector<Output>{interrupt("O")}));
    page.key(Rml::Input::KI_DELETE);
    assert(page.take() == (std::vector<Output>{interrupt("TTO")}));
    page.key(Rml::Input::KI_END);
    assert(page.take() == (std::vector<Output>{interrupt("blank")}));
    /* At the end already: nothing moved. */
    page.key(Rml::Input::KI_RIGHT);
    assert(page.take().empty());
    /* Selecting is not read. */
    page.key(Rml::Input::KI_LEFT, Rml::Input::KM_SHIFT);
    assert(page.take().empty());
    /* Typing over the selection with the character it holds is an edit,
     * though the value stays the same. */
    assert(page.type("o") == (std::vector<Output>{interrupt("TTO")}));
}

/* Up and Down move a field's cursor to an end before the launcher's step, and
 * reading the lines keeps focus on the field: the move is not read. */
void text_field_lines_keep_quiet(Rml::Context& context) {
    Page page(context);
    focus_player_name(page);
    page.key(Rml::Input::KI_HOME);
    assert(page.take() == (std::vector<Output>{interrupt("O")}));
    page.arrow(1);
    assert(page.take() == (std::vector<Output>{interrupt("Your connect code")}));
    page.arrow(-1);
    assert(page.take() ==
           (std::vector<Output>{interrupt("Player name, edit, OTTO. 1–8 letters or digits. "
                                          "Changes apply to the next search.")}));
    /* The cursor is where Up left it, at the start. */
    page.key(Rml::Input::KI_RIGHT);
    assert(page.take() == (std::vector<Output>{interrupt("T")}));
}

/* Leaving a field says nothing more, unless the launcher rewrites it as it
 * leaves: an emptied name gets the stored one back. */
void text_field_leaving_focus(Rml::Context& context) {
    Page page(context);
    focus_player_name(page);
    page.key(Rml::Input::KI_BACK);
    page.take();
    page.focus("net-target");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Friend's connect code, edit, blank. Leave blank to host your code. "
                              "Open Online, Direct Connect in-game.")}));
    page.focus("net-name");
    page.frame();
    page.take();
    page.key(Rml::Input::KI_END);
    for (int i = 0; i < 3; ++i) {
        page.key(Rml::Input::KI_BACK);
    }
    assert(
        page.take() == (std::vector<Output>{interrupt("OT"), interrupt("O"), interrupt("blank")}));
    /* The launcher's blur handler, refresh_online: the name stored last. */
    page.field("net-name", "O");
    page.focus("net-target");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Player name, O. Friend's connect code, edit, blank. "
                                          "Leave blank to host your code. Open Online, Direct "
                                          "Connect in-game.")}));
}

void settings_status_messages(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.focus("performance");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt(
               "Performance preset: Apply, button. For slow devices: 1x internal resolution, no "
               "anti-aliasing, 1x filtering and reverb off.")}));
    const std::string preset =
        "Performance preset: internal resolution 1x, anti-aliasing off, filtering 1x and "
        "reverb off. Anti-aliasing and filtering apply after a restart.";
    page.text("settings-status", preset);
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{queue(preset)}));
    page.text("settings-status", "Saved automatically.");
    page.run(a11y::kSettleTime);
    assert(page.take().empty());
}

/* Launcher::action("check-now") writes the same text to both status lines. */
void update_check_is_spoken_once(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(1);
    page.focus("check-now");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Check for updates now: Check now, button. Melee-PC is "
                                          "up to date (v0.1.8-beta).")}));
    page.status("Checking for updates...");
    page.text("check-status", "Checking for updates...");
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{queue("Checking for updates...")}));
    /* The result arrives after the player has moved to another tab. */
    page.show_tab(0);
    page.frame();
    page.take();
    page.text("check-status", "Update available: v0.2.0");
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{queue("Update available: v0.2.0")}));
}

/* Every control in the launcher, on every view and tab, has a name. Guards
 * against a base merge adding a row the reader cannot name. */
void every_control_has_a_name(Rml::Context& context) {
    Page page(context);
    int controls = 0;
    for (const char* tag : {"button", "input", "select", "textarea"}) {
        Rml::ElementList elements;
        page.document().GetElementsByTagName(elements, tag);
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
    assert(controls > 30); /* the walk found the page */
}

void other_documents_are_ignored(Rml::Context& context) {
    Page page(context, "port-menu.rml");
    page.run(a11y::kSettleTime * 8);
    assert(page.take().empty());
}

void unloading_stops_the_reader(Rml::Context& context) {
    Page page(context);
    page.open_without_disc();
    page.close();
    page.run(a11y::kSettleTime);
    assert(page.take().empty());
}

/* RmlUi shutting down while the plugin is still registered, then speech
 * stopping: the reverse of the game's order, which every test above follows. */
void rmlui_shutdown_before_stop(Rml::Context& context) {
    std::vector<Output> outputs;
    a11y::Speech speech(a11y::Config{true, false}, std::make_unique<FakeBridge>(outputs));
    speech.init();
    a11y::launcher_speech_start(speech);
    Rml::ElementDocument* document = context.LoadDocument(s_resources + "/launcher.rml");
    assert(document != nullptr);
    Rml::Shutdown();
    a11y::launcher_speech_frame(a11y::Clock::time_point() + a11y::kSettleTime * 2);
    a11y::launcher_speech_stop();
    speech.shutdown();
    assert(outputs.empty());
}

}  // namespace

/* Speech's log goes nowhere here; each Page turns it off anyway. */
extern "C" void pc_log_line(const char*, ...) {}

int main(int argc, char** argv) {
    assert(argc == 2);
    s_resources = argv[1];
    NullRender render;
    Rml::SetRenderInterface(&render);
    Rml::Initialise();
    Rml::LoadFontFace(s_resources + "/font.ttf");
    Rml::Context* context = Rml::CreateContext("main", Rml::Vector2i(1280, 720));
    assert(context != nullptr);

    opening_without_disc(*context);
    opening_waits_for_the_disc_check(*context);
    focus_change_is_announced(*context);
    returning_to_the_same_control_is_silent(*context);
    status_is_spoken_once_settled(*context);
    verify_reports_progress_in_steps(*context);
    update_banner_is_announced(*context);
    tab_becomes_down(*context);
    entering_and_leaving_settings(*context);
    tab_with_text_gives_a_hint(*context);
    controls_page_line_by_line(*context);
    online_page_lines_between_controls(*context);
    tab_switch_ends_reading(*context);
    speech_off_leaves_the_keys_alone(*context);
    cycling_setting_and_its_value(*context);
    slider_step(*context);
    text_field_typing(*context);
    text_field_cursor(*context);
    text_field_lines_keep_quiet(*context);
    text_field_leaving_focus(*context);
    settings_status_messages(*context);
    update_check_is_spoken_once(*context);
    every_control_has_a_name(*context);
    other_documents_are_ignored(*context);
    unloading_stops_the_reader(*context);
    rmlui_shutdown_before_stop(*context);

    std::cout << "launcher_speech: all tests passed\n";
    return 0;
}
