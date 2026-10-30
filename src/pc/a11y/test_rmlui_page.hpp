/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What the tests of the RmlUi pages' readers share: an RmlUi render interface
 * that draws nothing, a fake screen reader bridge that records what reaches
 * it, copies of the base port's listeners the pages rely on, and the
 * accessibility settings behind the Accessibility category. */
#pragma once
#include "a11y_hooks.h"
#include "accessibility_settings.hpp"
#include "cue_settings.hpp"
#include "match_announcement_settings.hpp"
#include "rmlui_reader.hpp"
#include "settings_page.hpp"
#include "speech.hpp"
#include "switches.hpp"
#include <RmlUi/Core.h>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rmlui_test {

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

inline Output interrupt(std::string text) {
    return Output{std::move(text), true};
}

inline Output queue(std::string text) {
    return Output{std::move(text), false};
}

constexpr std::chrono::milliseconds kFrame{16};

/* The menus' fonts, as the base port's load_menu_fonts (src/pc/launcher.cpp)
 * loads them: a text field lays out its text, and so places its cursor, only
 * in a face it has. */
inline void load_fonts(const std::string& resources) {
    Rml::LoadFontFace(resources + "/font.ttf", true);
    Rml::LoadFontFace(resources + "/font-bold.ttf");
    Rml::LoadFontFace(resources + "/menu.ttf");
    Rml::LoadFontFace(resources + "/menu-bold.ttf");
}

/* A copy of the base port's type_online (src/pc/launcher.cpp), which both
 * pages' Online fields share: typed text in an Online field loses a character
 * the field cannot store and goes in as capitals. */
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

/* The accessibility settings as pc_a11y_init wires them, enough for the
 * Accessibility category and its submenus, and no file: the switches first;
 * the cues, their shared values in Cue settings with the gap warning and the
 * ledge warning with its distance; the match announcements, with Match
 * announcement settings and Voice settings in it, which holds, in place of
 * the voices' rows, a choice of the tests' own, whose options, `voices`, are
 * asked at run time as the Voice rows' installed voices are, and a Preview
 * of the tests' own. The listener tells the switches, as the hooks' does.
 * What the rows ask of the game is recorded in `asked`: "preview <cue>",
 * "stop", "sound <n>". */
class A11ySettings {
public:
    explicit A11ySettings(a11y::Speech& speech)
        : settings(speech, "", false), switches(settings.model(), speech, false),
          cues(settings.model()) {
        cues.add_cue("gap_warning", "Gap warning", "Turns the gap warning on or off.", 1.0f);
        a11y::CueGroup ledge = cues.add_cue(
            "ledge_warning", "Ledge warning", "Turns the ledge warning on or off.", 1.0f);
        ledge.group.add_number(a11y::NumberSpec{"distance", "Ledge warning distance",
            "How far from the edge the warning starts, in stage units.", "units", 40.0f, 5.0f,
            100.0f, 1.0f, true});
        announcements = std::make_unique<a11y::MatchAnnouncementSettings>(settings.model());
        a11y::ValueGroup test =
            settings.model().group("test", "Tests").in(&announcements->voice_menu());
        test.add_choice(a11y::ChoiceSpec{
            .name = "voice",
            .label = "Test voice",
            .description = "A choice of the tests' own.",
            .default_option = "Microsoft Mark",
            .list = [this] { return voices; },
            .shown = true,
        });
        test.add_action(
            a11y::ActionSpec{"preview", "Preview test voice", "Says a line in the test voice."});
    }

    ~A11ySettings() { a11y::settings_pages_stop(); }

    /* Once speech has started, before a page loads: the settings read, the
     * switches started, which set speech (`speech_on` false: Speech saved
     * off), and the category's plugin registered. */
    void start(bool speech_on) {
        if (!speech_on) {
            settings.model().set(value("speech.on"), 0.0f);
        }
        settings.listen([this](const std::vector<const a11y::NamedValue*>&) { switches.update(); });
        settings.load(a11y::Clock::now());
        switches.start();
        a11y::settings_pages_start(settings,
            a11y::SettingsPageGame{
                [this](std::string_view cue) { asked.push_back("preview " + std::string(cue)); },
                [this] { asked.push_back("stop"); },
                [this](a11y::MenuSound sound) {
                    asked.push_back("sound " + std::to_string(static_cast<int>(sound)));
                },
            });
    }

    const a11y::NamedValue& value(std::string_view key) const {
        const a11y::NamedValue* named = settings.model().find(key);
        if (named == nullptr) {
            std::abort();
        }
        return *named;
    }

    /* What the rows asked of the game since the last take. */
    std::vector<std::string> take() { return std::exchange(asked, {}); }

    a11y::AccessibilitySettings settings;
    a11y::Switches switches;
    a11y::CueSettings cues;
    std::unique_ptr<a11y::MatchAnnouncementSettings> announcements;
    std::vector<std::string> asked;
    a11y::ChoiceOptions voices{{"Microsoft David", ""}, {"Microsoft Mark", ""},
        {"Microsoft Zira", "Microsoft Zira, English (United States)"}};
};

/* The Accessibility category's levels as A11ySettings builds them, by the
 * rows' ids. */
inline const std::vector<std::string> kTopRows{"a11y-accessibility-on", "a11y-speech-on",
    "a11y-steering-on", "a11y-cues-on", "a11y-cues-settings", "a11y-match_announcements-on",
    "a11y-match_announcements-settings"};
inline const std::vector<std::string> kCueRows{"a11y-cues-volume", "a11y-gap_warning-on",
    "a11y-gap_warning-volume", "a11y-gap_warning-preview", "a11y-ledge_warning-on",
    "a11y-ledge_warning-volume", "a11y-ledge_warning-preview", "a11y-ledge_warning-distance"};
inline const std::vector<std::string> kAnnouncementRows{"a11y-match_announcements-voices",
    "a11y-match_announcements-your_damage", "a11y-match_announcements-opponents_damage",
    "a11y-match_announcements-damage_interval", "a11y-match_announcements-your_stocks",
    "a11y-match_announcements-opponents_stocks", "a11y-match_announcements-teammates",
    "a11y-match_announcements-time_left"};
inline const std::vector<std::string> kVoiceRows{"a11y-test-voice", "a11y-test-preview"};

/* Whether the Accessibility category's page shows the element, as the
 * document now stands, whether or not the page itself is shown. */
inline bool shown(Rml::ElementDocument& document, const std::string& id) {
    document.UpdateDocument();
    const Rml::Element* element = document.GetElementById(id);
    for (; element != nullptr && element->GetId() != "page-accessibility";
        element = element->GetParentNode())
    {
        if (!element->IsVisible(false)) {
            return false;
        }
    }
    return element != nullptr;
}

/* Each of the rows shown, and every other row of the category hidden. */
inline bool level_shown(Rml::ElementDocument& document, const std::vector<std::string>& level) {
    for (const std::vector<std::string>* rows :
        {&kTopRows, &kCueRows, &kAnnouncementRows, &kVoiceRows})
    {
        for (const std::string& id : *rows) {
            if (shown(document, id) != (rows == &level)) {
                return false;
            }
        }
    }
    return pc_a11y_settings_rows() == level;
}

/* The title line's text, empty while it is hidden. */
inline std::string title_line(Rml::ElementDocument& document) {
    if (!shown(document, "a11y-menu-title")) {
        return "";
    }
    return a11y::element_text(*document.GetElementById("a11y-menu-title"));
}

/* The base port's hand-off from its Change listener (Launcher::ProcessEvent,
 * PortMenu::ProcessEvent): a slider of the fork's stepped by RmlUi or
 * dragged. */
class SettingsChange final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override { pc_a11y_settings_change(event); }
};

/* Records the keys that reach the page's own listener. */
class KeyLog final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        keys.push_back(event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN));
    }
    std::vector<int> keys;
};

}  // namespace rmlui_test
