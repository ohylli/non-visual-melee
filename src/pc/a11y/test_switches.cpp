/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The switches: their rows, the hierarchy, a live change of each through
 * the accessibility settings and their file, the environment's overrides,
 * the last announcement before silence and the first after it, and a glide
 * ended by Steering off. Wired as pc_a11y_init wires them; speech goes to a
 * fake screen reader bridge. This file supplies the pc_log_line that main.c
 * normally provides. Its one argument is a scratch folder. */
#include "accessibility_settings.hpp"
#include "cue_settings.hpp"
#include "speech.hpp"
#include "steerer.hpp"
#include "switches.hpp"
#include "value_file.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

using a11y::Clock;
namespace fs = std::filesystem;

std::vector<std::string> s_lines;
fs::path s_dir;

struct Bridge {
    int inits = 0;
    std::vector<std::string> spoken;
};

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    explicit FakeBridge(Bridge& bridge) : m_bridge(bridge) {}

    std::string init() override {
        m_bridge.inits++;
        return "Fake";
    }
    void shutdown() override {}
    std::string output(std::string_view utf8, bool) override {
        m_bridge.spoken.emplace_back(utf8);
        return "";
    }

private:
    Bridge& m_bridge;
};

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void write_file(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}

/* Rewrites a file the poll is to notice: a later write time, whatever the
 * file system's resolution. */
void edit_file(const fs::path& path, const std::string& text) {
    fs::file_time_type before = fs::last_write_time(path);
    write_file(path, text);
    fs::last_write_time(path, before + std::chrono::seconds(2));
}

bool logged(const std::string& line) {
    for (const std::string& l : s_lines) {
        if (l == line) {
            return true;
        }
    }
    return false;
}

/* The switches and the cues' values wired as pc_a11y_init wires them: the
 * switches registered first, speech silent until they start, the
 * environment's overrides before the load, and every change through the
 * listener to the switches and the cues' mix level. `file` is written into
 * the scratch folder first when not empty. */
struct Game {
    Bridge bridge;
    std::unique_ptr<a11y::Speech> speech;
    fs::path path;
    std::unique_ptr<a11y::AccessibilitySettings> settings;
    std::unique_ptr<a11y::Switches> switches;
    std::unique_ptr<a11y::CueSettings> cues;
    Clock::time_point now = Clock::now();

    explicit Game(const char* name, const std::string& file = "", a11y::Config config = {})
        : path(s_dir / name) {
        fs::remove(path);
        if (!file.empty()) {
            write_file(path, file);
        }
        s_lines.clear();
        config.speech = false;
        speech = std::make_unique<a11y::Speech>(config, std::make_unique<FakeBridge>(bridge));
        speech->init();
        settings = std::make_unique<a11y::AccessibilitySettings>(*speech, path.string(), true);
        switches = std::make_unique<a11y::Switches>(settings->model(), *speech, true);
        cues = std::make_unique<a11y::CueSettings>(settings->model());
        a11y::Switches::override_from(config, *settings);
        settings->listen([this](const std::vector<const a11y::NamedValue*>&) {
            switches->update();
            cues->set_allowed(switches->cues_allowed());
        });
        settings->load(now);
        switches->start();
        cues->set_allowed(switches->cues_allowed());
    }

    const a11y::NamedValue& named(const char* key) const { return *settings->model().find(key); }
    /* A menu's change. */
    void set(const char* key, bool on) { settings->set(named(key), on ? 1.0f : 0.0f, now); }
    /* A hand edit of the file, polled. */
    void edit(const std::string& from, const std::string& to) {
        std::string text = read_file(path);
        assert(text.find(from) != std::string::npos);
        text.replace(text.find(from), from.size(), to);
        edit_file(path, text);
        now += a11y::ValueFile::kPollInterval * 2;
        settings->poll(now);
    }
    const a11y::SwitchState& state() const { return switches->state(); }
    bool mix_on() const { return cues->mix_level().on; }
};

a11y::SwitchState state_of(bool accessibility, bool speech, bool steering, bool cues) {
    return a11y::SwitchState{accessibility, speech, steering, cues};
}

void the_switches_are_the_first_rows() {
    Game game("rows.ini");
    const std::vector<a11y::ShownRow>& rows = game.settings->shown();
    assert(rows.size() >= 5);
    assert(rows[0].value->key == "accessibility.on" && rows[0].value->label == "Accessibility");
    assert(rows[1].value->key == "speech.on" && rows[1].value->label == "Speech");
    assert(rows[2].value->key == "steering.on" && rows[2].value->label == "Steering");
    assert(rows[3].value->key == "cues.on" && rows[4].opens->label == "Cue settings");
    for (int i = 0; i < 4; ++i) {
        assert(rows[i].menu == nullptr);
    }
    for (int i = 0; i < 3; ++i) {
        assert(rows[i].value->kind == a11y::ValueKind::toggle);
        assert(rows[i].value->default_value == 1.0f && !rows[i].value->description.empty());
    }
    const std::string text = read_file(game.path);
    const std::size_t accessibility = text.find("# Accessibility\n");
    const std::size_t speech = text.find("# Speech\n");
    const std::size_t steering = text.find("# Steering\n");
    const std::size_t cues = text.find("# All cues\n");
    assert(accessibility < speech && speech < steering && steering < cues &&
           cues != std::string::npos);
    assert(text.find("\naccessibility.on = on\n") != std::string::npos);
    assert(text.find("\nspeech.on = on\n") != std::string::npos);
    assert(text.find("\nsteering.on = on\n") != std::string::npos);
}

/* Speech starts once the switches have been read, and only then is the
 * screen reader started. Saved off, it never is. */
void speech_waits_for_the_switches() {
    Game on("start_on.ini");
    assert(on.speech->enabled() && on.bridge.inits == 1 && on.bridge.spoken.empty());
    assert(on.state() == state_of(true, true, true, true));
    assert(logged("[a11y] switches start: accessibility on, speech on, steering on, cues on"));

    Game off("start_off.ini", "accessibility.on = off\n");
    assert(!off.speech->enabled() && off.bridge.inits == 0);
    assert(off.state() == state_of(false, false, false, false));
    assert(logged("[a11y] switches start: accessibility off, speech off, steering off, cues off"));
    off.speech->announce("Non-Visual Melee ready", a11y::Mode::interrupt);
    assert(off.bridge.spoken.empty());
    assert(logged("[a11y] speak interrupt (off): \"Non-Visual Melee ready\""));
}

/* A higher switch off silences the ones below it and leaves their saved
 * values as they were; back on, they are as saved. */
void accessibility_is_above_speech_steering_and_cues() {
    Game game("hierarchy.ini", "speech.on = off\n");
    assert(game.state() == state_of(true, false, true, true));
    game.set("accessibility.on", false);
    assert(game.state() == state_of(false, false, false, false));
    assert(!game.mix_on());
    assert(game.named("steering.on").value == 1.0f && game.named("cues.on").value == 1.0f);
    const std::string text = read_file(game.path);
    assert(text.find("\nsteering.on = on\n") != std::string::npos);
    assert(text.find("\ncues.on = on\n") != std::string::npos);
    game.set("accessibility.on", true);
    assert(game.state() == state_of(true, false, true, true));
    assert(game.mix_on());

    /* Cues off mutes the mix, its own switch below Accessibility. */
    game.set("cues.on", false);
    assert(game.state() == state_of(true, false, true, false) && !game.mix_on());
    game.set("accessibility.on", false);
    game.set("cues.on", true);
    assert(!game.state().cues && !game.mix_on());
    game.set("accessibility.on", true);
    assert(game.state().cues && game.mix_on());
}

/* Each switch is live, from a menu or a hand edit. */
void every_switch_changes_while_the_game_runs() {
    Game game("live.ini");
    game.set("steering.on", false);
    assert(game.state() == state_of(true, true, false, true));
    assert(read_file(game.path).find("\nsteering.on = off\n") != std::string::npos);
    game.edit("steering.on = off", "steering.on = on");
    assert(game.state().steering);
    game.edit("cues.on = on", "cues.on = off");
    assert(!game.state().cues && !game.mix_on());
    game.set("cues.on", true);
    assert(game.state().cues && game.mix_on());
    game.edit("speech.on = on", "speech.on = off");
    assert(!game.state().speech && !game.speech->enabled());
    game.set("speech.on", true);
    assert(game.state().speech && game.speech->enabled());
    game.edit("accessibility.on = on", "accessibility.on = off");
    assert(game.state() == state_of(false, false, false, false) && !game.speech->enabled());
    game.set("accessibility.on", true);
    assert(game.state() == state_of(true, true, true, true) && game.speech->enabled());
    /* A change to a value under no switch leaves them be. */
    std::size_t lines = s_lines.size();
    game.settings->set(game.named("cues.volume"), 50.0f, game.now);
    assert(game.state() == state_of(true, true, true, true));
    for (std::size_t i = lines; i < s_lines.size(); ++i) {
        assert(s_lines[i].rfind("[a11y] switches", 0) != 0);
    }
}

/* Speech or Accessibility off says so as the last announcement, and nothing
 * reaches the screen reader after it; back on, the switch says so first. */
void the_last_announcement_before_silence() {
    Game game("last.ini");
    game.set("speech.on", false);
    assert((game.bridge.spoken == std::vector<std::string>{"Speech off"}));
    game.speech->announce("Off", a11y::Mode::interrupt);
    assert(game.bridge.spoken.size() == 1);
    assert(logged("[a11y] speak interrupt (off): \"Off\""));
    game.set("speech.on", true);
    assert(game.bridge.spoken.back() == "Speech on");

    game.set("accessibility.on", false);
    assert(game.bridge.spoken.back() == "Accessibility off");
    game.edit("speech.on = on", "speech.on = off");
    game.set("accessibility.on", true);
    /* Speech is still off by its own switch: nothing said. */
    assert(game.bridge.spoken.back() == "Accessibility off" && !game.speech->enabled());
    game.set("speech.on", true);
    assert(game.bridge.spoken.back() == "Speech on");
    /* Accessibility back on with speech on says Accessibility. */
    game.set("accessibility.on", false);
    game.set("accessibility.on", true);
    assert(game.bridge.spoken.back() == "Accessibility on");
    assert(game.bridge.inits == 1);
    /* With speech off already, Accessibility off says nothing. */
    std::size_t said = game.bridge.spoken.size();
    game.set("speech.on", false);
    game.set("accessibility.on", false);
    assert(game.bridge.spoken.size() == said + 1 && game.bridge.spoken.back() == "Speech off");
}

/* The environment's overrides hold for the run in both directions and are
 * never written; MELEE_A11Y_STEER and MELEE_A11Y_CUES hold against
 * Accessibility too, so an agent run steers and plays cues with speech
 * off. A menu's change lifts one and is saved. */
void the_environment_overrides_for_the_run() {
    a11y::Config agent;
    agent.accessibility = false;
    agent.steering = true;
    agent.cues = true;
    Game game("agent.ini", "", agent);
    assert(game.state() == state_of(false, false, true, true) && game.mix_on());
    assert(!game.speech->enabled() && game.bridge.inits == 0);
    assert(logged("[a11y] switches start: accessibility off (MELEE_A11Y=0), speech off, steering "
                  "on (MELEE_A11Y_STEER=1), cues on (MELEE_A11Y_CUES=1)"));
    std::string text = read_file(game.path);
    assert(text.find("\naccessibility.on = on\n") != std::string::npos);
    /* A hand edit of an overridden switch waits under it. */
    game.edit("steering.on = on", "steering.on = off");
    assert(game.state().steering);
    /* A menu's change lifts it: Steering follows the file under
     * Accessibility again. */
    game.set("steering.on", true);
    assert(!game.named("steering.on").overridden && !game.state().steering);
    assert(read_file(game.path).find("\nsteering.on = on\n") != std::string::npos);
    game.set("accessibility.on", true);
    assert(game.state() == state_of(true, true, true, true) && game.speech->enabled());
    assert(read_file(game.path).find("\naccessibility.on = on\n") != std::string::npos);

    /* MELEE_A11Y=1 forces Accessibility on over a file that turned it off. */
    a11y::Config back;
    back.accessibility = true;
    Game rescued("rescued.ini", "accessibility.on = off\n", back);
    assert(rescued.state() == state_of(true, true, true, true) && rescued.speech->enabled());
    assert(read_file(rescued.path).find("accessibility.on = off\n") != std::string::npos);

    /* Off overrides: steering and cues off with Accessibility on. */
    a11y::Config off;
    off.steering = false;
    off.cues = false;
    Game quiet("quiet.ini", "", off);
    assert(quiet.state() == state_of(true, true, false, false) && !quiet.mix_on());
    text = read_file(quiet.path);
    assert(text.find("\nsteering.on = on\n") != std::string::npos);
    assert(text.find("\ncues.on = on\n") != std::string::npos);
}

/* Steering off in the middle of a glide: the glide ends where the cursor
 * is, and the next pad has no stick of the glide's (spec open question 6). */
void steering_off_mid_glide_puts_the_stick_back_to_rest() {
    s_lines.clear();
    a11y::Steerer<int> steerer(a11y::kStageCursorMovement, [](const int&) { return "Corneria"; });
    a11y::Stick stick;
    steerer.start(7, a11y::Point{20.0f, 0.0f});
    steerer.frame(a11y::Point{0.0f, 0.0f}, a11y::Stick{});
    assert(steerer.take_stick(&stick) && stick.x > 0);
    steerer.frame(a11y::Point{0.0f, 0.0f}, a11y::Stick{});
    steerer.switch_off("steering was switched off");
    assert(!steerer.active() && steerer.status().phase == a11y::GlidePhase::ended);
    assert(!steerer.take_stick(&stick));
    assert(!steerer.take_stick(&stick));
    /* Back on, a new glide starts afresh. */
    steerer.reported();
    steerer.start(7, a11y::Point{20.0f, 0.0f});
    steerer.frame(a11y::Point{0.0f, 0.0f}, a11y::Stick{});
    assert(steerer.take_stick(&stick) && stick.x > 0);
}

}  // namespace

extern "C" void pc_log_line(const char* fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    s_lines.emplace_back(line);
}

int main(int argc, char** argv) {
    s_dir = argc > 1 ? fs::path(argv[1]) : fs::path("switches_test");
    fs::create_directories(s_dir);
    the_switches_are_the_first_rows();
    speech_waits_for_the_switches();
    accessibility_is_above_speech_steering_and_cues();
    every_switch_changes_while_the_game_runs();
    the_last_announcement_before_silence();
    the_environment_overrides_for_the_run();
    steering_off_mid_glide_puts_the_stick_back_to_rest();
    std::cout << "switches: all tests passed\n";
    return 0;
}
