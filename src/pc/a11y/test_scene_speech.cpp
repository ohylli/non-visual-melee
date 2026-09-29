/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The scene table against a fake screen reader bridge. Links scene_speech.cpp
 * and speech.cpp only, so this file supplies the pc_log_line that main.c
 * normally provides. */
#include "scene_speech.hpp"
#include "speech.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::vector<std::string> s_lines;

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

/* Speech over a fake bridge, and scene speech over that; s_lines only ever
 * holds the lines of the test that is running. */
struct Fixture {
    std::vector<Output> outputs;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(outputs)};
    a11y::SceneSpeech scenes{speech};

    Fixture() {
        speech.init();
        s_lines.clear();
    }
};

int kind(a11y::SceneKind scene_kind) {
    return static_cast<int>(scene_kind);
}

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

void named_scene_interrupts() {
    Fixture f;
    f.scenes.entered(kind(a11y::SceneKind::GS_RESULTS));
    assert((f.outputs == std::vector<Output>{{"Results. No speech yet.", true}}));
    assert(count_logged("[a11y] speak interrupt: \"Results. No speech yet.\"") == 1);
}

void title_screen_names_its_way_out() {
    Fixture f;
    f.scenes.entered(kind(a11y::SceneKind::GS_TITLE));
    assert((f.outputs == std::vector<Output>{{"Title screen. Press Start.", true}}));
}

void silent_scene_says_and_logs_nothing() {
    Fixture f;
    f.scenes.entered(kind(a11y::SceneKind::GS_MENU));
    f.scenes.entered(kind(a11y::SceneKind::GS_CSS));
    f.scenes.entered(kind(a11y::SceneKind::GS_VS));
    assert(f.outputs.empty());
    assert(s_lines.empty());
}

void unknown_kind_is_silent_and_logged_once() {
    Fixture f;
    f.scenes.entered(200);
    f.scenes.entered(200);
    f.scenes.entered(201);
    assert(f.outputs.empty());
    assert(count_logged("[a11y] scene 200: not in the scene table, silent") == 1);
    assert(count_logged("[a11y] scene 201: not in the scene table, silent") == 1);
    assert(s_lines.size() == 2);
}

void same_kind_twice_is_announced_twice() {
    Fixture f;
    f.scenes.entered(kind(a11y::SceneKind::GS_RESULTS));
    f.scenes.entered(kind(a11y::SceneKind::GS_RESULTS));
    assert(f.outputs.size() == 2);
    assert(f.outputs[0] == f.outputs[1]);
}

void lookup_tells_silent_from_unknown() {
    assert(a11y::scene_announcement(kind(a11y::SceneKind::GS_SSS)) ==
           "Stage select. No speech yet. Press Start for a random stage.");
    assert(a11y::scene_announcement(kind(a11y::SceneKind::GS_TRAINING)) == "");
    assert(!a11y::scene_announcement(-1).has_value());
    assert(!a11y::scene_announcement(kind(a11y::SceneKind::GS_ONLINE_LOBBY) + 1).has_value());
}

}  // namespace

extern "C" void pc_log_line(const char* fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    s_lines.emplace_back(line);
}

int main() {
    named_scene_interrupts();
    title_screen_names_its_way_out();
    silent_scene_says_and_logs_nothing();
    unknown_kind_is_silent_and_logged_once();
    same_kind_twice_is_announced_twice();
    lookup_tells_silent_from_unknown();
    std::cout << "scene_speech: all tests passed\n";
    return 0;
}
