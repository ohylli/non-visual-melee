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
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string s_resources;

class NullRender final : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override {
        return 1;
    }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String&) override {
        dimensions = {1, 1};
        return 1;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override {
        return 1;
    }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};

struct Output {
    std::string text;
    bool interrupt;

    bool operator==(const Output&) const = default;
};

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    explicit FakeBridge(std::vector<Output>& outputs) : m_outputs(outputs) {}

    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view utf8, bool interrupt) override {
        m_outputs.push_back(Output{std::string(utf8), interrupt});
        return "";
    }

private:
    std::vector<Output>& m_outputs;
};

Output interrupt(std::string text) {
    return Output{std::move(text), true};
}

Output queue(std::string text) {
    return Output{std::move(text), false};
}

std::vector<Output> none() {
    return {};
}

constexpr std::chrono::milliseconds kFrame{16};

constexpr const char* kOpening =
    "PC settings. Graphics, tab, selected, 1 of 5. Left and right to switch tabs";
constexpr const char* kResume = "Return to game, button";

/* A copy of the base port's type_online: typed text in an Online field loses
 * a character the field cannot store and goes in as capitals. */
class OnlineFilter final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        Rml::Element* target = event.GetTargetElement();
        const Rml::String id = target != nullptr ? target->GetId() : "";
        if (id != "net-name" && id != "net-target") {
            return;
        }
        const Rml::String text = event.GetParameter<Rml::String>("text", "");
        Rml::String typed = text;
        for (char& c : typed) {
            if (c >= 'a' && c <= 'z') {
                c -= 'a' - 'A';
            }
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    (id == "net-target" && c == '#')))
            {
                event.StopPropagation();
                return;
            }
        }
        if (typed != text) {
            event.StopPropagation();
            Rml::Dictionary parameters = event.GetParameters();
            parameters["text"] = typed;
            target->DispatchEvent(Rml::EventId::Textinput, parameters);
        }
    }
};

/* The port menu with port menu speech attached, changed through the same
 * calls PortMenu makes. Loaded hidden, as pc_menu_init loads it when the game
 * starts. */
class Menu {
public:
    explicit Menu(Rml::Context& context, bool speech = true)
        : m_context(context),
          m_speech(a11y::Config{speech, false}, std::make_unique<FakeBridge>(m_outputs)) {
        m_speech.init();
        a11y::port_menu_speech_start(m_speech);
        m_document = m_context.LoadDocument(s_resources + "/port-menu.rml");
        assert(m_document != nullptr);
        m_document->AddEventListener(Rml::EventId::Textinput, &m_filter, true);
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
        label("axis-name-0", "Stick Left");
        label("axis-0", "leftx -");
    }

    /* PortMenu::toggle */
    void toggle() {
        m_open = !m_open;
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
        static constexpr const char* pages[] = {
            "page-graphics", "page-audio", "page-cheats", "page-controls", "page-online"};
        m_tab = (next + 5) % 5;
        for (int i = 0; i < 5; ++i) {
            element(kTabs[i])->SetClass("selected", i == m_tab);
            element(pages[i])->SetClass("selected", i == m_tab);
        }
        focus(kTabs[m_tab]);
    }

    /* PortMenu::focus_step with its hook: Up (-1) or Down (1) through the
     * tab, the page's controls (PortMenu::page_focus) and Return to game. */
    void step(int direction) {
        if (a11y::port_menu_speech_step(direction)) {
            return;
        }
        const std::vector<std::string> ids = focus_ids();
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

    /* Opens the menu and takes the opening. */
    void open() {
        toggle();
        frame();
        take();
    }

    /* One frame of the game loop: the menu's frame with its hook, then RmlUi
     * updates the page as the next frame draws it. */
    void frame() {
        m_now += kFrame;
        a11y::port_menu_speech_frame(m_open, m_now);
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
        m_document->Close();
        m_context.Update();
        m_document = nullptr;
    }

private:
    static constexpr const char* kTabs[] = {
        "tab-graphics", "tab-audio", "tab-cheats", "tab-controls", "tab-online"};

    /* PortMenu::show_tab's focus_ids, from PortMenu::page_focus. */
    std::vector<std::string> focus_ids() const {
        std::vector<std::string> ids{kTabs[m_tab]};
        switch (m_tab) {
        case 0:
            ids.insert(ids.end(), {"display", "sync", "resolution", "aspect", "hud-mode", "aa",
                                      "filter", "filter-mode", "custom-textures", "backend"});
            break;
        case 1:
            ids.insert(ids.end(), {"volume", "music-volume", "sfx-volume", "mute", "reverb", "fps",
                                      "scale", "port-check-update"});
            break;
        case 2:
            ids.insert(ids.end(), {"unlock-all", "frozen-stadium", "free-camera", "ucf"});
            break;
        case 4:
            ids.insert(ids.end(), {"net-name", "net-target", "net-delay"});
            break;
        default:
            ids.push_back("pad-port");
            for (int i = 0; i < 12; ++i) {
                ids.push_back("bind-" + std::to_string(i));
            }
            for (int i = 0; i < 10; ++i) {
                ids.push_back("axis-" + std::to_string(i));
            }
            ids.push_back("bind-reset");
            break;
        }
        ids.push_back("resume");
        return ids;
    }

    Rml::Context& m_context;
    OnlineFilter m_filter;
    int m_tab = 0;
    bool m_open = false;
    std::vector<Output> m_outputs;
    a11y::Speech m_speech;
    Rml::ElementDocument* m_document = nullptr;
    a11y::Clock::time_point m_now;
};

/* Silent until opened; the opening interrupts, and so does the closing. */
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

/* The menu opens on the tab used last; Return to game closes it too. */
void reopening_on_the_last_tab(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.show_tab(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Audio & interface, tab, selected, 2 of 5. Left and right to switch "
                              "tabs")}));
    menu.toggle();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Back to game")}));
    menu.toggle();
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "PC settings. Audio & interface, tab, selected, 2 of 5. Left and "
                              "right to switch tabs")}));
    menu.step(-1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kResume)}));
    menu.toggle(); /* PortMenu::apply("resume") */
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Back to game")}));
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

/* Every tab's first control, read from the tab with Down. */
void every_tabs_first_control(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    const std::vector<std::vector<Output>> expected{
        {interrupt("Display mode, Windowed, Enter to change")},
        {interrupt("Master volume, slider, 100%. Left and right to adjust. Left/right or drag "
                   "to adjust.")},
        {interrupt("Unlock everything, Normal, Enter to change. Unlock all 26 characters, 11 "
                   "secret stages, and All-Star mode.")},
        {interrupt("Controller, Port 1, Enter to change. No controller in this port.")},
        {interrupt("Player name, edit, PLAYER. 1–8 letters or digits. Changes apply to the next "
                   "search.")},
    };
    const char* tabs[] = {"Graphics, tab, selected, 1 of 5",
        "Audio & interface, tab, selected, 2 of 5", "Cheats, tab, selected, 3 of 5",
        "Controls, tab, selected, 4 of 5", "Online, tab, selected, 5 of 5"};
    for (int tab = 0; tab < 5; ++tab) {
        menu.show_tab(tab);
        menu.frame();
        if (tab > 0) {
            /* No page opens with text, so no hint. */
            assert(menu.take() ==
                   (std::vector<Output>{
                       interrupt(std::string(tabs[tab]) + ". Left and right to switch tabs")}));
        }
        menu.step(1);
        menu.frame();
        assert(menu.take() == expected[static_cast<size_t>(tab)]);
    }
}

/* Enter cycles a value: the new value alone is said, and "Saved." never. */
void setting_and_its_value(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.focus("sync");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt("Vertical sync, On, Enter to change")}));
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
    assert(menu.take() ==
           (std::vector<Output>{interrupt("Anti-aliasing RESTART, 4x MSAA, Enter to change")}));
}

/* Left and Right step a slider; Enter cycles it, wrapping from the top. */
void slider_steps_and_wraps(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.show_tab(1);
    menu.focus("volume");
    menu.frame();
    menu.take();
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
}

/* An Online field echoes typing and explains a refused character. */
void text_field_typing(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.show_tab(4);
    menu.focus("net-name");
    menu.frame();
    menu.take();
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

/* Controls: a heading between the buttons and axes, and the keyboard's lines
 * between Restore defaults and Return to game. */
void controls_lines(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.show_tab(3);
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
    const std::vector<std::string> lines{"Keyboard", "WASD, arrows Move", "X Attack", "Z Special",
        "C, V Jump", "Q, E Shield", "Tab Grab", "Enter Start", "IJKL C-stick"};
    for (const std::string& line : lines) {
        menu.step(1);
        menu.frame();
        assert(menu.take() == (std::vector<Output>{interrupt(line)}));
    }
    assert(menu.focused() == menu.element("bind-reset"));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kResume)}));
    menu.step(-1);
    assert(menu.take() == (std::vector<Output>{interrupt(lines.back())}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kResume)}));
    assert(menu.focused() == menu.element("resume"));
}

/* Online: the connect code between the fields, the closing note after Input
 * delay. */
void online_lines(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.show_tab(4);
    menu.focus("net-name");
    menu.frame();
    menu.take();
    menu.step(1);
    assert(menu.take() == (std::vector<Output>{interrupt("Your connect code")}));
    menu.step(1);
    assert(menu.take() == (std::vector<Output>{interrupt("PLAYER#AB12")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Friend's connect code, edit, blank. Leave blank to host your code. "
                              "Open Online, Direct Connect in-game.")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() ==
           (std::vector<Output>{interrupt("Input delay, Auto, Enter to change. Automatic adapts "
                                          "to your connection. Changes apply next match.")}));
    menu.step(1);
    assert(menu.take() ==
           (std::vector<Output>{interrupt("Internet discovery can take a few minutes. Some home "
                                          "networks cannot connect without a relay.")}));
    menu.step(1);
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(kResume)}));
}

/* The status line is spoken queued once it settles, except "Saved.", which
 * follows every change, and what it shows as the menu opens. */
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
    /* Closed and opened again on the same text: only the opening. */
    menu.toggle();
    menu.frame();
    menu.take();
    menu.status("Saved automatically.");
    menu.frame();
    menu.toggle();
    menu.frame();
    menu.run(a11y::kSettleTime * 2);
    assert(menu.take() == (std::vector<Output>{interrupt(kOpening)}));
}

/* PortMenu::apply("port-check-update") writes the same text to both regions;
 * the result shows when the next save refreshes the page. */
void update_check(Rml::Context& context) {
    Menu menu(context);
    menu.open();
    menu.show_tab(1);
    menu.focus("port-check-update");
    menu.frame();
    assert(menu.take() == (std::vector<Output>{interrupt(
                              "Check for updates: Check now, button. Up to date (v0.1.8-beta)")}));
    menu.label("port-update-status", "Checking for updates...");
    menu.status("Checking for updates...");
    menu.run(a11y::kSettleTime);
    assert(menu.take() == (std::vector<Output>{queue("Checking for updates...")}));
    /* The result arrives after the player has moved to another tab. */
    menu.show_tab(0);
    menu.frame();
    menu.take();
    menu.label("port-update-status", "Update available: v0.2.0");
    menu.status("Saved.");
    menu.run(a11y::kSettleTime);
    assert(menu.take() == (std::vector<Output>{queue("Update available: v0.2.0")}));
}

/* Records the keys that reach the menu's own listener. */
class KeyLog final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        keys.push_back(event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN));
    }
    std::vector<int> keys;
};

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
    menu.open();
    menu.show_tab(4);
    menu.focus("net-name");
    menu.frame();
    menu.step(1);
    assert(menu.focused() == menu.element("net-target"));
}

/* Every control in the menu, on every tab, has a name. Guards against a base
 * merge adding a row the reader cannot name. */
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
        a11y::port_menu_speech_frame(true, now);
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
    Rml::LoadFontFace(s_resources + "/font.ttf");
    Rml::Context* context = Rml::CreateContext("main", Rml::Vector2i(1280, 720));
    assert(context != nullptr);

    opening_and_closing(*context);
    reopening_on_the_last_tab(*context);
    closed_menu_is_silent(*context);
    every_tabs_first_control(*context);
    setting_and_its_value(*context);
    slider_steps_and_wraps(*context);
    text_field_typing(*context);
    controls_lines(*context);
    online_lines(*context);
    status_line(*context);
    update_check(*context);
    tab_becomes_down(*context);
    speech_off_leaves_the_keys_alone(*context);
    every_control_has_a_name(*context);
    other_documents_are_ignored(*context);
    unloading_stops_the_reader(*context);

    Rml::Shutdown();
    std::cout << "port_menu_speech: all tests passed\n";
    return 0;
}
