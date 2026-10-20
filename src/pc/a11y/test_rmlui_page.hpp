/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What the tests of the RmlUi pages' readers share: an RmlUi render interface
 * that draws nothing, a fake screen reader bridge that records what reaches
 * it, and copies of the base port's listeners the pages rely on. */
#pragma once
#include "speech.hpp"
#include <RmlUi/Core.h>
#include <chrono>
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

/* Records the keys that reach the page's own listener. */
class KeyLog final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        keys.push_back(event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN));
    }
    std::vector<int> keys;
};

}  // namespace rmlui_test
