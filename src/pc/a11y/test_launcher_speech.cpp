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
#include <cstring>
#include <iostream>
#include <memory>
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

/* The launcher's tab_ids, with the fork's Accessibility before Launcher. */
constexpr const char* kTabs[] = {"tab-graphics", "tab-audio", "tab-cheats", "tab-controls",
    "tab-online", "tab-accessibility", "tab-launcher"};
constexpr int kTabCount = 7;
constexpr int kAccessibilityTab = 5;
constexpr int kLauncherTab = 6;

/* What the openings and Settings say, again and again. */
constexpr const char* kChoose =
    "Choose Disc, button. Pick a Melee disc image: ISO, GCM, CISO, RVZ, GCZ or WIA.";
constexpr const char* kSettings = "Settings, button. Graphics, audio, gameplay, controls, online "
                                  "play and the launcher itself.";
constexpr const char* kPlayerName = "Player name, edit, OTTO. 1-8 letters or digits. Changes "
                                    "apply to the next search.";
constexpr const char* kFriendsCode = "Friend's connect code, edit, blank. Leave blank to host "
                                     "your code. Open Online, then Direct Connect, in the game.";
constexpr const char* kControls = "Controls. The keyboard layout and controller setup.";
constexpr const char* kAccessibility =
    "Accessibility. Speech, steering and the audio cues of Non-Visual Melee.";

/* One launcher page with launcher speech attached, changed through the same
 * calls launcher.cpp makes. */
class Page {
public:
    explicit Page(Rml::Context& context, const char* file = "launcher.rml", bool speech = true)
        : m_context(context),
          m_speech(a11y::Config{speech, false}, std::make_unique<FakeBridge>(m_outputs)),
          m_a11y(m_speech) {
        m_speech.init();
        m_a11y.start(speech);
        a11y::launcher_speech_start(m_speech);
        m_document = m_context.LoadDocument(s_resources + "/" + file);
        assert(m_document != nullptr);
        m_document->AddEventListener(Rml::EventId::Textinput, &m_filter, true);
        m_document->AddEventListener(Rml::EventId::Change, &m_change);
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
    rmlui_test::A11ySettings& a11y() { return m_a11y; }

    Rml::Element* element(const char* id) {
        Rml::Element* e = m_document->GetElementById(id);
        assert(e != nullptr);
        return e;
    }

    Rml::Element* focused() { return m_context.GetFocusElement(); }

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

    /* Launcher::note */
    void note(const std::string& value) {
        text("settings-status", value);
        element("settings-status")->SetProperty("display", value.empty() ? "none" : "block");
    }

    /* refresh_online's write to a text field */
    void field(const char* id, const std::string& value) {
        auto* e = dynamic_cast<Rml::ElementFormControl*>(element(id));
        assert(e != nullptr);
        e->SetValue(value);
    }

    /* Focus moved by the launcher, with what its focus listener does
     * (Launcher::describe): a category taking focus opens its page. */
    void focus(const char* id) {
        const bool focused = element(id)->Focus();
        assert(focused);
        for (int i = 0; i < kTabCount; ++i) {
            if (m_settings && std::strcmp(id, kTabs[i]) == 0 && i != m_tab) {
                select_tab(i);
            }
        }
    }

    /* Launcher::select_tab */
    void select_tab(int tab) {
        m_tab = tab;
        for (int i = 0; i < kTabCount; ++i) {
            element(kTabs[i])->SetClass("selected", i == tab);
            const std::string page = std::string("page-") + (kTabs[i] + 4);
            element(page.c_str())->SetClass("selected", i == tab);
        }
    }

    /* Launcher::show_tab */
    void show_tab(int tab) {
        select_tab(tab);
        focus(kTabs[tab]);
    }

    /* Launcher::enter_rows, with its hook: Right or Enter on a category. */
    void enter_rows() {
        m_document->UpdateDocument();
        for (const std::string& id : page_focus()) {
            Rml::Element* e = element(id.c_str());
            if (e->IsVisible(true) && !e->IsPseudoClassSet("disabled")) {
                focus(id.c_str());
                return;
            }
        }
        a11y::launcher_speech_enter();
    }

    /* Left (-1) or Right (1) on one of the fork's rows: Launcher::horizontal's
     * hand-off, then one turn of the loop. */
    void horizontal(int direction) {
        const bool handed = pc_a11y_settings_step(m_document, focused()->GetId(), direction);
        assert(handed);
        frame();
    }

    /* Enter, A or a click on one of the fork's rows: Launcher::action's
     * hand-off (from activate_focus, or RmlUi's own click on Enter), then one
     * turn of the loop. */
    void press() {
        const bool handed = pc_a11y_settings_press(m_document, focused()->GetId());
        assert(handed);
        frame();
    }

    /* Launcher::horizontal's Left, with its hook, on a category, where the
     * launcher itself does nothing. */
    void left() { a11y::launcher_speech_back(); }

    /* Launcher::back, with its hooks: Escape or B. */
    void back() {
        if (pc_a11y_settings_back(m_document)) {
            return;
        }
        if (a11y::launcher_speech_back()) {
            return;
        }
        if (!m_settings) {
            return;
        }
        if (in_rows()) {
            show_tab(m_tab);
        } else {
            show_settings(false);
        }
    }

    /* Launcher::focus_step, with its hook: Up (-1) or Down (1) through the
     * column that holds focus (Launcher::column), the categories and Back or
     * the open page's rows, wrapping around. */
    void step(int direction) {
        if (a11y::launcher_speech_step(direction)) {
            return;
        }
        std::vector<std::string> ids;
        if (in_rows()) {
            ids = page_focus();
        } else {
            ids.assign(std::begin(kTabs), std::end(kTabs));
            ids.push_back("back");
        }
        const int count = static_cast<int>(ids.size());
        int index = direction > 0 ? -1 : 0;
        for (int i = 0; i < count; ++i) {
            if (element(ids[i].c_str()) == focused()) {
                index = i;
            }
        }
        for (int i = 0; i < count; ++i) {
            index = (index + direction + count) % count;
            Rml::Element* e = element(ids[index].c_str());
            if (e->IsVisible(true) && !e->IsPseudoClassSet("disabled")) {
                focus(ids[index].c_str());
                return;
            }
        }
    }

    /* Launcher::show_settings, with the parts of refresh_settings the tests
     * hear from. */
    void show_settings(bool show) {
        m_settings = show;
        m_document->SetClass("in-settings", show);
        if (!show) {
            focus("settings");
            return;
        }
        text("display", "Windowed");
        text("sync", "On");
        text("aa", "4x MSAA");
        text("volume-val", "100%");
        field("net-name", "OTTO");
        text("net-code", "ABCD#123");
        text("check-status", "Melee-PC is up to date (v0.1.8-beta).");
        note("");
        show_tab(m_tab);
    }

    /* Opens with a disc, then enters Settings. */
    void open_settings() {
        open_with_disc();
        show_settings(true);
        frame();
        take();
    }

    /* Opens with a disc, enters Settings and the rows of a category. */
    void open_rows(int tab) {
        open_settings();
        show_tab(tab);
        enter_rows();
        frame();
        take();
    }

    /* The launcher's constructor with no disc remembered, then the opening. */
    void open_without_disc() {
        enabled("play", false);
        enabled("verify", false);
        focus("choose");
        run(a11y::kSettleTime);
        assert(take() == (std::vector<Output>{
                             queue(std::string("Melee launcher. No disc selected. ") + kChoose)}));
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
                             "Melee launcher. Ready to play, Disc not verified. Play, button. "
                             "Start Melee from this disc. Disc: melee.iso, Super Smash Bros. "
                             "Melee, USA, Revision 2 (1.02)")}));
    }

    /* One turn of the launcher's loop: RmlUi updates the page, then the hook. */
    void frame() {
        m_now += kFrame;
        m_context.Update();
        a11y::settings_pages_frame(a11y::SettingsHost::launcher, true);
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
        m_document->RemoveEventListener(Rml::EventId::Change, &m_change);
        m_document->Close();
        m_context.Update();
        m_document = nullptr;
    }

private:
    /* Launcher::in_rows */
    bool in_rows() {
        for (Rml::Element* e = focused(); e != nullptr; e = e->GetParentNode()) {
            if (e->GetId() == "settings-content") {
                return true;
            }
        }
        return false;
    }

    /* Launcher::page_focus */
    std::vector<std::string> page_focus() const {
        switch (m_tab) {
        case 0:
            return {"display", "sync", "resolution", "aspect", "hud-mode", "aa", "filter",
                "filter-mode", "fps", "custom-textures", "backend", "performance"};
        case 1:
            return {"volume", "music-volume", "sfx-volume", "mute", "reverb"};
        case 2:
            return {"unlock-all", "frozen-stadium", "free-camera", "ucf"};
        case 4:
            return {"net-name", "net-target", "net-delay"};
        case 5:
            return pc_a11y_settings_rows();
        case 6:
            return {"scale", "check-updates", "check-now", "settings-discord", "export-logs"};
        default:
            return {};
        }
    }

    Rml::Context& m_context;
    OnlineFilter m_filter;
    rmlui_test::SettingsChange m_change;
    int m_tab = 0;
    bool m_settings = false;
    std::vector<Output> m_outputs;
    a11y::Speech m_speech;
    rmlui_test::A11ySettings m_a11y;
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
    assert(page.take() == (std::vector<Output>{
                              queue(std::string("Melee launcher. No disc selected. ") + kChoose)}));
}

/* Also the Play announcement, which carries the disc. */
void opening_waits_for_the_disc_check(Rml::Context& context) {
    Page page(context);
    page.open_with_disc();
}

/* Home's buttons say what they do, from the description bar's text. */
void focus_change_is_announced(Rml::Context& context) {
    Page page(context);
    page.open_with_disc();
    page.focus("choose");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kChoose)}));
    page.focus("discord");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{
               interrupt("Discord, button. Join the community Discord server in your browser.")}));
}

/* The file dialog disables Choose Disc, which parks focus on its container,
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
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Verify Disc, button. Check the disc image against the original "
                              "NTSC-U 1.02 disc.")}));

    /* Launcher::action("verify"), then Launcher::controls: busy, so
     * everything else is disabled. */
    for (const char* id : {"play", "choose", "settings", "discord"}) {
        page.enabled(id, false);
    }
    page.text("verify-label", "Cancel Verify");
    page.focus("verify");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Cancel Verify")}));

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
    page.text("verify-label", "Verify Disc");
    page.focus("verify");
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{interrupt("Verify Disc"),
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
    page.document().SetClass("in-settings", true);
    page.run(a11y::kSettleTime);
    page.document().SetClass("in-settings", false);
    page.run(a11y::kSettleTime);
    assert(page.take().empty());

    page.focus("update-action");
    page.frame();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Update Now, button. Download the new version.")}));
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
    assert(page.focused() == page.element("choose"));
}

/* Arriving in Settings says how to move there; coming back to a category
 * does not. */
void entering_and_leaving_settings(Rml::Context& context) {
    Page page(context);
    page.open_with_disc();
    page.focus("settings");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kSettings)}));
    page.show_settings(true);
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Settings. Graphics. Up and down to choose, right to enter. "
                              "How the game looks.")}));
    page.enter_rows();
    page.frame();
    page.take();
    page.back();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Graphics. How the game looks.")}));
    page.back();
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{interrupt(kSettings)}));
}

/* Up and Down walk the categories and Back, around; the count leaves Back
 * out. */
void categories_and_back(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    const auto down = [&page](const std::string& said) {
        page.step(1);
        page.frame();
        assert(page.take() == (std::vector<Output>{interrupt(said)}));
    };
    down("Audio. Volume and reverb.");
    down("Gameplay. Unlocks, stage hazards and controller fixes.");
    down(kControls);
    down("Online. Your player name, connect codes and input delay.");
    down(kAccessibility);
    down("Launcher. The size of these menus, updates and the community.");
    down("Back, button. Back to the main menu.");
    down("Graphics. How the game looks.");
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Back, button. Back to the main menu.")}));
}

/* Headings, line breaks and " · " separators each end a line. The page has
 * no rows, so Right or Enter on its category enters its text instead, Up and
 * Down read the lines around, and Escape or Left go back to the category,
 * focus on the category all along. */
void controls_page_line_by_line(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(3);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kControls)}));
    const std::vector<std::string> lines{"Keyboard", "WASD, arrows Move", "X Attack", "Z Special",
        "C, V Jump", "Q, E Shield", "Tab Grab", "Enter Start", "IJKL C-stick", "Controller",
        "Remap a gamepad from the in-game menu: press BACK on the pad, or F1, then open "
        "Controls. Controllers are only detected once the game is running."};
    page.enter_rows();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(lines.front())}));
    for (size_t i = 1; i < lines.size(); ++i) {
        page.step(1);
        page.frame();
        assert(page.take() == (std::vector<Output>{interrupt(lines[i])}));
    }
    assert(page.focused() == page.element("tab-controls"));
    page.step(-1);
    assert(page.take() == (std::vector<Output>{interrupt("Controller")}));
    /* Pressed again, the line being read. */
    page.enter_rows();
    assert(page.take() == (std::vector<Output>{interrupt("Controller")}));
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt(lines.back())}));
    /* The lines wrap around, as the rows of other pages do. */
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt(lines.front())}));
    page.step(-1);
    assert(page.take() == (std::vector<Output>{interrupt(lines.back())}));

    /* Escape back to the category, which stays in Settings; Down then moves
     * on to the next category. */
    page.back();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kControls)}));
    assert(page.focused() == page.element("tab-controls"));
    page.back();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kSettings)}));
    page.show_settings(true);
    page.frame();
    page.take();
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Online. Your player name, connect codes and input delay.")}));
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kControls)}));

    /* Entered afresh from the first line; Left goes back too. */
    page.enter_rows();
    assert(page.take() == (std::vector<Output>{interrupt(lines.front())}));
    page.step(1);
    page.take();
    page.left();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kControls)}));
    page.left();
    page.frame();
    assert(page.take().empty());
    page.enter_rows();
    assert(page.take() == (std::vector<Output>{interrupt(lines.front())}));

    /* Moving off the category ends the reading. */
    page.focus("tab-online");
    page.frame();
    page.take();
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kControls)}));
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Gameplay. Unlocks, stage hazards and controller fixes.")}));
}

/* A page with rows reads its lines only from its rows: Down on its category
 * moves to the next one. */
void category_with_rows_moves_on(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(4);
    page.frame();
    page.take();
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kAccessibility)}));
}

/* The lines sit between the rows, in the page's order, and the rows wrap
 * around, the lines after the last one included. */
void online_page_lines_between_rows(Rml::Context& context) {
    Page page(context);
    page.open_rows(4);
    assert(page.focused() == page.element("net-name"));
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt("Your connect code, ABCD#123")}));
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kFriendsCode)}));
    page.step(-1);
    assert(page.take() == (std::vector<Output>{interrupt("Your connect code, ABCD#123")}));
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kPlayerName)}));

    const std::string note = "Internet discovery can take a few minutes. Some home networks "
                             "cannot connect without a relay.";
    /* Up from the first row: the line after the last row, then that row. */
    page.step(-1);
    assert(page.take() == (std::vector<Output>{interrupt(note)}));
    page.step(-1);
    page.frame();
    assert(page.focused() == page.element("net-delay"));
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Input delay, Auto, Left and right to change. Automatic adapts to "
                              "your connection. Changes apply next match.")}));
    /* And Down from the last row past the line, around to the first. */
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt(note)}));
    page.step(1);
    page.frame();
    assert(page.focused() == page.element("net-name"));
    assert(page.take() == (std::vector<Output>{interrupt(kPlayerName)}));

    page.focus("net-delay");
    page.frame();
    page.take();
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt(note)}));
    /* A value change on the control focus is on ends the reading; the next
     * press starts from that control again. */
    page.text("net-delay", "1 frames");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("1 frames")}));
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt(note)}));

    /* A line rewritten while it is held: the next press starts from the
     * control again. */
    page.focus("net-name");
    page.frame();
    page.take();
    page.step(1);
    page.take();
    page.text("net-code", "WXYZ#999");
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt("Your connect code, WXYZ#999")}));
    page.text("net-code", "ABCD#123");
}

/* Leaving the rows while on a line: the category is announced, and the next
 * press starts afresh. */
void leaving_the_rows_ends_reading(Rml::Context& context) {
    Page page(context);
    page.open_rows(4);
    page.step(1);
    page.take();
    page.back();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Online. Your player name, connect codes and input delay.")}));
    page.enter_rows();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kPlayerName)}));
    page.step(1);
    assert(page.take() == (std::vector<Output>{interrupt("Your connect code, ABCD#123")}));
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
    assert(page.focused() == page.element("tab-online"));
}

void cycling_setting_and_its_value(Rml::Context& context) {
    Page page(context);
    page.open_rows(0);
    assert(page.focused() == page.element("display"));
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{
                              interrupt("Vertical sync, On, Left and right to change. Synchronise "
                                        "presentation with your display.")}));
    page.text("sync", "Off");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Off")}));
    /* A badge is part of the name. */
    page.focus("aa");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Anti-aliasing RESTART, 4x MSAA, Left and right to change. Smooth "
                              "polygon edges with multisampling. Applies after a restart.")}));
}

/* Launcher::step_slider writes the slider and the span beside it. */
void slider_step(Rml::Context& context) {
    Page page(context);
    page.open_rows(1);
    assert(page.focused() == page.element("volume"));
    page.focus("music-volume");
    page.frame();
    page.take();
    page.focus("volume");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Master volume, slider, 100%. Left and right to adjust. Everything "
                              "the game plays. Left and right adjust it.")}));
    page.element("volume")->SetAttribute("value", 95.0f);
    page.text("volume-val", "95%");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("95%")}));
}

/* Focuses the Player name field, holding OTTO, with the cursor at its end. */
void focus_player_name(Page& page) {
    page.open_settings();
    page.show_tab(4);
    page.enter_rows();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kPlayerName)}));
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
    assert(page.take() == (std::vector<Output>{interrupt("Your connect code, ABCD#123")}));
    page.arrow(-1);
    assert(page.take() == (std::vector<Output>{interrupt(kPlayerName)}));
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
    assert(page.take() == (std::vector<Output>{interrupt(kFriendsCode)}));
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
           (std::vector<Output>{interrupt(std::string("Player name, O. ") + kFriendsCode)}));
}

/* Launcher::action("performance") writes its note under the rows. */
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
    page.note(preset);
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{queue(preset)}));
    page.note("");
    page.run(a11y::kSettleTime);
    assert(page.take().empty());
}

/* Launcher::action("check-now") writes the same text to both status lines.
 * The row's own line, the check's result, is read with the row. */
void update_check_is_spoken_once(Rml::Context& context) {
    Page page(context);
    page.open_rows(kLauncherTab);
    page.focus("check-now");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(
                              "Updates: Check now, button. Melee-PC is up to date (v0.1.8-beta). "
                              "Look for a new release now.")}));
    page.status("Checking for updates...");
    page.text("check-status", "Checking for updates...");
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{queue("Checking for updates...")}));
    /* The result arrives after the player has moved to another category. */
    page.show_tab(0);
    page.frame();
    page.take();
    page.text("check-status", "Update available: v0.2.0");
    page.run(a11y::kSettleTime);
    assert(page.take() == (std::vector<Output>{queue("Update available: v0.2.0")}));
}

/* The fork's category: its rows, built from the accessibility settings, read
 * as any page's rows are, from the first to the last and around. */
constexpr const char* kAccessibilityRow =
    "Accessibility, On, Left and right to change. Turns everything Non-Visual Melee adds on or "
    "off: speech, steering and the cues.";
constexpr const char* kSpeechRow =
    "Speech, On, Left and right to change. Turns speech on or off, through the screen reader "
    "and the voices.";
constexpr const char* kTestVoiceRow =
    "Test voice, Microsoft Mark, Left and right to change. A choice of the tests' own.";

/* A row opening a submenu: its name, how to open it and its description,
 * with no role word. */
constexpr const char* kCueSettingsRow =
    "Cue settings, Enter to open. Cue volume, and each cue's switch, volume and preview.";
constexpr const char* kAnnouncementSettingsRow =
    "Match announcement settings, Enter to open. The voices, and what is announced: damage, "
    "stocks, teammates and the time left.";
constexpr const char* kVoiceSettingsRow =
    "Voice settings, Enter to open. Your voice and the opponents' voice: each one's voice, rate, "
    "volume and a preview.";
constexpr const char* kCueVolumeRow =
    "Cue volume, slider, 100%. Left and right to adjust. The loudness of all cues together, "
    "under the master volume; the game's sound effect and music volumes leave cues alone.";
constexpr const char* kLedgeDistanceRow =
    "Ledge warning distance, slider, 40 units. Left and right to adjust. How far from the edge "
    "the warning starts, in stage units.";
constexpr const char* kTestPreviewRow =
    "Preview test voice: Play, button. Says a line in the test voice.";

/* Opening a submenu: its name, then its first row's announcement. */
std::string opening(std::string_view submenu, std::string_view row) {
    return std::string(submenu) + ". " + std::string(row);
}

/* The top level: the master switches, each followed by the row opening its
 * submenu, read as any page's rows are, from the first to the last and
 * around. */
void accessibility_category_rows(Rml::Context& context) {
    Page page(context);
    page.open_settings();
    page.show_tab(kAccessibilityTab);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kAccessibility)}));
    page.enter_rows();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kAccessibilityRow)}));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
    assert(rmlui_test::title_line(page.document()).empty());
    const std::vector<std::string> said{kSpeechRow,
        "Steering, On, Left and right to change. The D-pad steps the hand on character select and "
        "the cursor on stage select. Off, the D-pad does what the game makes of it, quick chat "
        "included.",
        "Cues, On, Left and right to change. Turns every cue on or off.", kCueSettingsRow,
        "Match announcements, On, Left and right to change. Says what the HUD shows during a "
        "match: your damage and stocks in your voice, everyone else's in the opponents' voice, "
        "and the time left on the screen reader.",
        kAnnouncementSettingsRow, kAccessibilityRow};
    for (const std::string& text : said) {
        page.step(1);
        page.frame();
        assert(page.take() == (std::vector<Output>{interrupt(text)}));
    }
    /* Escape: back to the category. */
    page.back();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kAccessibility)}));
}

/* A submenu opens on its first row by Enter, A or a click, or by Right, its
 * title line naming it, and only its rows shown; Up and Down go round them;
 * Left and Right still step a value; Escape, B, or Left on a Preview go
 * back to the row that opened it, and on the top level Escape goes on back
 * to the category. */
void accessibility_submenu_moving(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    page.focus("a11y-cues-settings");
    page.frame();
    page.take();
    page.press();
    assert(page.focused() == page.element("a11y-cues-volume"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kCueRows));
    assert(rmlui_test::title_line(page.document()) == "Cue settings");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(opening("Cue settings", kCueVolumeRow))}));
    /* Round from the first to the last, and back. */
    page.step(-1);
    assert(page.focused() == page.element("a11y-ledge_warning-distance"));
    page.step(1);
    assert(page.focused() == page.element("a11y-cues-volume"));
    for (std::size_t i = 1; i < rmlui_test::kCueRows.size(); ++i) {
        page.step(1);
        assert(page.focused() == page.element(rmlui_test::kCueRows[i].c_str()));
    }
    page.step(1);
    assert(page.focused() == page.element("a11y-cues-volume"));
    /* A value still steps either way. */
    page.horizontal(-1);
    assert(page.a11y().value("cues.volume").value == 95.0f);
    page.horizontal(1);
    assert(page.a11y().value("cues.volume").value == 100.0f);
    assert(page.focused() == page.element("a11y-cues-volume"));
    /* Left on a Preview: back, and Right opens again. */
    page.focus("a11y-gap_warning-preview");
    page.horizontal(-1);
    assert(page.focused() == page.element("a11y-cues-settings"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
    assert(rmlui_test::title_line(page.document()).empty());
    page.horizontal(1);
    assert(page.focused() == page.element("a11y-cues-volume"));
    page.take();
    /* Right on a Preview is the page's: nothing. */
    page.focus("a11y-gap_warning-preview");
    assert(!pc_a11y_settings_step(&page.document(), "a11y-gap_warning-preview", 1));
    /* Escape or B: back, then on to the category. */
    page.back();
    assert(page.focused() == page.element("a11y-cues-settings"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kCueSettingsRow)}));
    page.back();
    assert(page.focused() == page.element("tab-accessibility"));
    /* Left on an opening row of the top level is the page's: back to the
     * category. */
    assert(!pc_a11y_settings_step(&page.document(), "a11y-cues-settings", -1));
}

/* Voice settings, inside Match announcement settings: two levels in, and
 * out by Left on a row holding no value and by Escape. */
void accessibility_nested_submenu(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    page.focus("a11y-match_announcements-settings");
    page.press();
    assert(page.focused() == page.element("a11y-match_announcements-voices"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kAnnouncementRows));
    assert(rmlui_test::title_line(page.document()) == "Match announcement settings");
    page.press();
    assert(page.focused() == page.element("a11y-test-voice"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kVoiceRows));
    assert(
        rmlui_test::title_line(page.document()) == "Match announcement settings / Voice settings");
    page.step(1);
    assert(page.focused() == page.element("a11y-test-preview"));
    page.step(1);
    assert(page.focused() == page.element("a11y-test-voice"));
    page.step(-1);
    assert(page.focused() == page.element("a11y-test-preview"));
    page.press();
    assert(page.a11y().take() == (std::vector<std::string>{"preview test"}));
    /* Left on the Preview: one level up, onto Voice settings, the preview
     * stopped as its row lost focus. */
    page.horizontal(-1);
    assert(page.focused() == page.element("a11y-match_announcements-voices"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kAnnouncementRows));
    assert(page.a11y().take() == (std::vector<std::string>{"stop"}));
    /* Left on the row opening Voice settings: up to the top level. */
    page.horizontal(-1);
    assert(page.focused() == page.element("a11y-match_announcements-settings"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
    /* In by Right twice, out by Escape twice. */
    page.horizontal(1);
    page.horizontal(1);
    assert(page.focused() == page.element("a11y-test-voice"));
    page.back();
    assert(page.focused() == page.element("a11y-match_announcements-voices"));
    page.back();
    assert(page.focused() == page.element("a11y-match_announcements-settings"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
}

/* What the submenus say: an opening row its name, how to open it and its
 * description; opening one, by Enter or Right, its name and its first row,
 * interrupting; going back, the opening row, as any move onto it. Up and Down
 * go round the rows, each read as it is, the title line never a stop; Settings left
 * inside a submenu and entered again says no submenu's name. */
void accessibility_submenu_words(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    page.focus("a11y-cues-settings");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kCueSettingsRow)}));
    page.press();
    assert(page.take() == (std::vector<Output>{interrupt(opening("Cue settings", kCueVolumeRow))}));
    /* Up from the first row: the last, not the title line above the first. */
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kLedgeDistanceRow)}));
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kCueVolumeRow)}));
    /* Round the whole submenu, each press onto the next row, said alone. */
    const std::vector<std::string>& rows = rmlui_test::kCueRows;
    for (std::size_t i = 1; i <= rows.size(); ++i) {
        page.step(1);
        page.frame();
        assert(page.focused() == page.element(rows[i % rows.size()].c_str()));
        const std::vector<Output> said = page.take();
        assert(said.size() == 1 && said[0].interrupt);
        assert(said[0].text.rfind("Cue settings", 0) == std::string::npos);
    }
    assert(page.focused() == page.element("a11y-cues-volume"));
    /* Escape or B: the opening row. */
    page.back();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kCueSettingsRow)}));
    /* Right opens too; Left on a Preview goes back. */
    page.horizontal(1);
    assert(page.take() == (std::vector<Output>{interrupt(opening("Cue settings", kCueVolumeRow))}));
    page.focus("a11y-gap_warning-preview");
    page.frame();
    page.take();
    page.horizontal(-1);
    assert(page.take() == (std::vector<Output>{interrupt(kCueSettingsRow)}));
    /* Voice settings, two levels in: each level's name as it is entered,
     * the inner one's alone; out, each opening row. */
    page.focus("a11y-match_announcements-settings");
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kAnnouncementSettingsRow)}));
    page.press();
    assert(page.take() == (std::vector<Output>{interrupt(
                              opening("Match announcement settings", kVoiceSettingsRow))}));
    page.press();
    assert(
        page.take() == (std::vector<Output>{interrupt(opening("Voice settings", kTestVoiceRow))}));
    page.step(-1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kTestPreviewRow)}));
    page.back();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kVoiceSettingsRow)}));
    /* In again from the submenu it is inside: its name again. */
    page.horizontal(1);
    assert(
        page.take() == (std::vector<Output>{interrupt(opening("Voice settings", kTestVoiceRow))}));
    page.back();
    page.back();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kAnnouncementSettingsRow)}));
    /* Settings left inside a submenu: entered again, the top level, and a
     * submenu entered then says its name. */
    page.press();
    page.take();
    page.show_settings(false);
    page.frame();
    page.take();
    page.show_settings(true);
    page.frame();
    page.take();
    page.enter_rows();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kAccessibilityRow)}));
    page.focus("a11y-match_announcements-settings");
    page.frame();
    page.take();
    page.press();
    assert(page.take() == (std::vector<Output>{interrupt(
                              opening("Match announcement settings", kVoiceSettingsRow))}));
}

/* Leaving Settings from inside a submenu, or the pointer taking focus out of
 * the page, brings the category back to its top level: the next opening
 * is there. */
void accessibility_submenu_left_by_leaving(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    page.focus("a11y-match_announcements-settings");
    page.press();
    page.press();
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kVoiceRows));
    page.show_settings(false);
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
    page.show_settings(true);
    assert(page.focused() == page.element("tab-accessibility"));
    page.enter_rows();
    assert(page.focused() == page.element("a11y-accessibility-on"));
    /* The pointer onto Back, or a click on a category. */
    page.focus("a11y-cues-settings");
    page.press();
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kCueRows));
    page.focus("back");
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
    page.focus("a11y-cues-settings");
    page.press();
    page.show_tab(0);
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
    /* A click inside the open submenu stays in it. */
    page.show_tab(kAccessibilityTab);
    page.enter_rows();
    page.focus("a11y-cues-settings");
    page.press();
    page.focus("a11y-gap_warning-on");
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kCueRows));
}

/* With speech off, a sighted player moves through the submenus just the
 * same. */
void accessibility_submenus_with_speech_off(Rml::Context& context) {
    Page page(context, "launcher.rml", false);
    page.enabled("play", false);
    page.enabled("verify", false);
    page.focus("choose");
    page.run(a11y::kSettleTime * 2);
    page.show_settings(true);
    page.show_tab(kAccessibilityTab);
    page.enter_rows();
    page.focus("a11y-cues-settings");
    page.press();
    assert(page.focused() == page.element("a11y-cues-volume"));
    page.step(-1);
    assert(page.focused() == page.element("a11y-ledge_warning-distance"));
    page.step(1);
    assert(page.focused() == page.element("a11y-cues-volume"));
    page.back();
    assert(page.focused() == page.element("a11y-cues-settings"));
    assert(rmlui_test::level_shown(page.document(), rmlui_test::kTopRows));
    assert(page.take().empty());
}

/* A switch flips with Left, Right, Enter or A, and says its new value, as
 * the base port's two-state settings do; the change reaches the settings. */
void accessibility_switch(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    page.step(1);
    page.step(1);
    page.frame();
    page.take();
    assert(page.focused() == page.element("a11y-steering-on"));
    page.horizontal(1);
    assert(page.take() == (std::vector<Output>{interrupt("Off")}));
    assert(page.a11y().value("steering.on").value == 0.0f);
    page.horizontal(-1);
    assert(page.take() == (std::vector<Output>{interrupt("On")}));
    page.press();
    assert(page.take() == (std::vector<Output>{interrupt("Off")}));
    assert(!page.a11y().switches.state().steering);
    /* The launcher plays no sounds. */
    assert(page.a11y().take().empty());
}

/* A slider steps by its value's step, with the D-pad through the hand-off
 * and with the keyboard's arrows through RmlUi's own slider and its Change;
 * Enter leaves it as it is. */
void accessibility_slider(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    page.focus("a11y-cues-volume");
    page.frame();
    page.take();
    page.horizontal(-1);
    assert(page.take() == (std::vector<Output>{interrupt("95%")}));
    assert(page.a11y().value("cues.volume").value == 95.0f);
    page.key(Rml::Input::KI_LEFT);
    assert(page.take() == (std::vector<Output>{interrupt("90%")}));
    assert(page.a11y().value("cues.volume").value == 90.0f);
    page.press();
    assert(page.take().empty());
    assert(page.a11y().value("cues.volume").value == 90.0f);
    /* Held at the top of the range. */
    page.focus("a11y-gap_warning-volume");
    page.frame();
    page.take();
    page.horizontal(1);
    assert(page.take().empty());
    assert(page.a11y().value("gap_warning.volume").value == 100.0f);
    /* A value in stage units, a unit a step. */
    page.focus("a11y-ledge_warning-distance");
    page.frame();
    page.take();
    page.horizontal(1);
    assert(page.take() == (std::vector<Output>{interrupt("41 units")}));
    /* A hand edit, polled, shows on the page and is said as the new value. */
    page.a11y().settings.model().set(page.a11y().value("ledge_warning.distance"), 30.0f);
    a11y::settings_pages_frame(a11y::SettingsHost::launcher, true);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("30 units")}));
}

/* A choice steps through its options with Left and Right, round from
 * either end, and to the next with Enter or A, said as the option alone;
 * the row shows an option by its label. The options may change while the
 * page is open: the row then shows the option in use, the saved text kept. */
void accessibility_choice(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    /* Focus put straight on a row of a submenu, as only a test can, says
     * the submenu's name too. */
    page.focus("a11y-test-voice");
    page.frame();
    assert(
        page.take() == (std::vector<Output>{interrupt(opening("Voice settings", kTestVoiceRow))}));
    page.horizontal(1);
    assert(
        page.take() == (std::vector<Output>{interrupt("Microsoft Zira, English (United States)")}));
    assert(page.a11y().value("test.voice").text == "Microsoft Zira");
    page.horizontal(1);
    assert(page.take() == (std::vector<Output>{interrupt("Microsoft David")}));
    page.horizontal(-1);
    assert(
        page.take() == (std::vector<Output>{interrupt("Microsoft Zira, English (United States)")}));
    page.press();
    assert(page.take() == (std::vector<Output>{interrupt("Microsoft David")}));
    assert(page.a11y().value("test.voice").text == "Microsoft David");
    /* The launcher plays no sounds. */
    assert(page.a11y().take().empty());
    /* David uninstalled: the default's option shows, David stays saved. */
    page.a11y().voices.erase(page.a11y().voices.begin());
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Microsoft Mark")}));
    assert(page.a11y().value("test.voice").text == "Microsoft David");
    page.horizontal(1);
    assert(
        page.take() == (std::vector<Output>{interrupt("Microsoft Zira, English (United States)")}));
    /* None listed: the row says so, and a step changes nothing. */
    page.a11y().voices.clear();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("None")}));
    page.horizontal(1);
    page.press();
    assert(page.take().empty());
    assert(page.a11y().value("test.voice").text == "Microsoft Zira");
}

/* Preview plays its cue on Enter or A, again from its start when pressed
 * again, and says nothing: the cue is the answer. Leaving the row stops it.
 * Its Left goes back a level (accessibility_submenu_moving). */
void accessibility_preview(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    page.focus("a11y-gap_warning-preview");
    page.frame();
    page.take();
    page.press();
    assert(page.take().empty());
    assert(page.a11y().take() == (std::vector<std::string>{"preview gap_warning"}));
    a11y::settings_pages_frame(a11y::SettingsHost::launcher, true);
    assert(page.a11y().take().empty());
    page.press();
    assert(page.a11y().take() == (std::vector<std::string>{"preview gap_warning"}));
    page.step(1);
    page.frame();
    a11y::settings_pages_frame(a11y::SettingsHost::launcher, true);
    assert(page.a11y().take() == (std::vector<std::string>{"stop"}));
    /* Leaving the launcher stops it too. */
    page.focus("a11y-ledge_warning-preview");
    page.press();
    assert(page.a11y().take() == (std::vector<std::string>{"preview ledge_warning"}));
    page.close();
    assert(page.a11y().take() == (std::vector<std::string>{"stop"}));
}

/* Speech or Accessibility switched off from its row says so last; on again,
 * it says so first and the row announces itself again, once. */
void accessibility_switches_speech(Rml::Context& context) {
    Page page(context);
    page.open_rows(kAccessibilityTab);
    page.step(1);
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt(kSpeechRow)}));
    page.press();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Speech off")}));
    page.press();
    assert(page.take() == (std::vector<Output>{interrupt("Speech on"), queue(kSpeechRow)}));
    page.run(a11y::kSettleTime * 2);
    assert(page.take().empty());
    page.step(-1);
    page.frame();
    page.take();
    page.press();
    page.frame();
    assert(page.take() == (std::vector<Output>{interrupt("Accessibility off")}));
    page.press();
    assert(page.take() ==
           (std::vector<Output>{interrupt("Accessibility on"), queue(kAccessibilityRow)}));
}

/* Every control in the launcher, on every view and page, has a name. Guards
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
    auto settings = std::make_unique<rmlui_test::A11ySettings>(speech);
    settings->start(true);
    a11y::launcher_speech_start(speech);
    Rml::ElementDocument* document = context.LoadDocument(s_resources + "/launcher.rml");
    assert(document != nullptr);
    Rml::Shutdown();
    a11y::launcher_speech_frame(a11y::Clock::time_point() + a11y::kSettleTime * 2);
    a11y::settings_pages_frame(a11y::SettingsHost::launcher, true);
    a11y::launcher_speech_stop();
    settings.reset();
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
    rmlui_test::load_fonts(s_resources);
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
    categories_and_back(*context);
    controls_page_line_by_line(*context);
    category_with_rows_moves_on(*context);
    online_page_lines_between_rows(*context);
    leaving_the_rows_ends_reading(*context);
    speech_off_leaves_the_keys_alone(*context);
    cycling_setting_and_its_value(*context);
    slider_step(*context);
    text_field_typing(*context);
    text_field_cursor(*context);
    text_field_lines_keep_quiet(*context);
    text_field_leaving_focus(*context);
    settings_status_messages(*context);
    update_check_is_spoken_once(*context);
    accessibility_category_rows(*context);
    accessibility_submenu_moving(*context);
    accessibility_nested_submenu(*context);
    accessibility_submenu_left_by_leaving(*context);
    accessibility_submenus_with_speech_off(*context);
    accessibility_submenu_words(*context);
    accessibility_switch(*context);
    accessibility_slider(*context);
    accessibility_choice(*context);
    accessibility_preview(*context);
    accessibility_switches_speech(*context);
    every_control_has_a_name(*context);
    other_documents_are_ignored(*context);
    unloading_stops_the_reader(*context);
    rmlui_shutdown_before_stop(*context);

    std::cout << "launcher_speech: all tests passed\n";
    return 0;
}
