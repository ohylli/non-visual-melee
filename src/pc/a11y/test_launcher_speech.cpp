/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Launcher speech against the real resources/launcher.rml, loaded into an
 * RmlUi context whose render interface draws nothing. Each test changes the
 * page the way src/pc/launcher.cpp does, runs the reader's frame step and
 * checks what reached a fake screen reader bridge. It is the guard against a
 * base merge that reshapes the page. The resources directory is argv[1]. */
#include "launcher_speech.hpp"
#include "speech.hpp"
#include <RmlUi/Core.h>
#include <cassert>
#include <cstdarg>
#include <cstdio>
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

constexpr std::chrono::milliseconds kFrame{16};

/* One launcher page with launcher speech attached, changed through the same
 * calls launcher.cpp makes. */
class Page {
public:
    explicit Page(Rml::Context& context, const char* file = "launcher.rml")
        : m_context(context),
          m_speech(a11y::Config{true, false}, std::make_unique<FakeBridge>(m_outputs)) {
        m_speech.init();
        a11y::launcher_speech_start(m_speech);
        m_document = m_context.LoadDocument(s_resources + "/" + file);
        assert(m_document != nullptr);
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

    void close() {
        m_document->Close();
        m_context.Update();
        m_document = nullptr;
    }

private:
    Rml::Context& m_context;
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

/* Records the keys that reach the launcher's own listener. */
class KeyLog final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        keys.push_back(event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN));
    }
    std::vector<int> keys;
};

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
    assert(context.LoadDocument(s_resources + "/launcher.rml") != nullptr);
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
    other_documents_are_ignored(*context);
    unloading_stops_the_reader(*context);
    rmlui_shutdown_before_stop(*context);

    std::cout << "launcher_speech: all tests passed\n";
    return 0;
}
