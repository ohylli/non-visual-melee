/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The voices' rows: where they sit, their options and words, the samples a
 * change plays, a missing voice, and OneCore unavailable. Wired as
 * pc_a11y_init wires them, over the accessibility settings' file in a
 * scratch folder, the voices' worker the real one over a fake voice bridge,
 * speech over a fake screen reader, and the cue engine rendered into a
 * buffer, a frame at a time on a fake clock. This file supplies the
 * pc_log_line that main.c normally provides. Its one argument is a scratch
 * folder. */
#include "accessibility_settings.hpp"
#include "cue_engine.hpp"
#include "speech.hpp"
#include "switches.hpp"
#include "test_voice_bridge.hpp"
#include "value_file.hpp"
#include "voice_names.hpp"
#include "voice_settings.hpp"
#include "voices.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
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
using a11y::Voice;
using a11y::VoicesState;
using a11y::test::FakeVoiceBridge;
using a11y::test::VoiceCalls;
namespace fs = std::filesystem;

std::vector<std::string> s_lines;
fs::path s_dir;

/* What the screen reader was handed: the text, and whether it interrupted. */
using Spoken = std::vector<std::pair<std::string, bool>>;

class FakeScreenReader final : public a11y::ScreenReaderBridge {
public:
    explicit FakeScreenReader(Spoken& spoken) : m_spoken(spoken) {}
    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view utf8, bool interrupt) override {
        m_spoken.emplace_back(std::string(utf8), interrupt);
        return {};
    }

private:
    Spoken& m_spoken;
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

bool starts_with(const std::string& line, const std::string& start) {
    return line.compare(0, start.size(), start) == 0;
}

bool logged(const std::string& start) {
    for (const std::string& line : s_lines) {
        if (starts_with(line, start)) {
            return true;
        }
    }
    return false;
}

/* The file holds this whole line. */
bool has_line(const std::string& file, const std::string& line) {
    return ("\n" + file).find("\n" + line + "\n") != std::string::npos;
}

int count(const std::string& start) {
    int n = 0;
    for (const std::string& line : s_lines) {
        n += starts_with(line, start) ? 1 : 0;
    }
    return n;
}

/* The accessibility settings, the switches with the voices' rows after
 * Speech, the voices and their rows, as pc_a11y_init wires them: speech
 * silent until the switches start it, the rows attached once the file is
 * read, every later change through the listener. `file`, when not empty, is
 * the file as the run finds it; `voices` the voices installed. */
struct Game {
    Spoken spoken;
    VoiceCalls calls;
    fs::path path;
    std::unique_ptr<a11y::Speech> speech;
    std::unique_ptr<a11y::AccessibilitySettings> settings;
    std::unique_ptr<a11y::VoiceSettings> rows;
    std::unique_ptr<a11y::Switches> switches;
    a11y::CueEngine engine;
    std::unique_ptr<a11y::Voices> voices;
    Clock::time_point now = Clock::now();
    /* The frames are the launcher's; false, the game's. */
    bool launcher = true;
    /* The loudest sample the engine has rendered since it was last reset. */
    float loudest = 0.0f;

    explicit Game(const char* name, const std::string& file = "",
        std::vector<a11y::VoiceInfo> installed = a11y::test::several_languages(),
        std::string open_error = "")
        : path(s_dir / name) {
        fs::remove(path);
        if (!file.empty()) {
            write_file(path, file);
        }
        s_lines.clear();
        a11y::Config config;
        config.speech = false;
        speech = std::make_unique<a11y::Speech>(config, std::make_unique<FakeScreenReader>(spoken));
        speech->init();
        settings = std::make_unique<a11y::AccessibilitySettings>(*speech, path.string(), true);
        switches = std::make_unique<a11y::Switches>(settings->model(), *speech, true);
        /* A submenu of the tests' own, in place of Voice settings. */
        const a11y::NamedMenu& menu =
            settings->model()
                .group("test", "Tests")
                .add_menu(a11y::MenuSpec{"voices", "Voice settings", "The voices."});
        rows = std::make_unique<a11y::VoiceSettings>(settings->model(), *speech, &menu);
        voices = std::make_unique<a11y::Voices>(*speech, engine,
            std::make_unique<FakeVoiceBridge>(calls, std::move(open_error), std::move(installed)),
            true, false);
        settings->listen([this](const std::vector<const a11y::NamedValue*>& changed) {
            switches->update();
            rows->changed(changed, now);
        });
        settings->load(now);
        rows->attach(*voices);
        switches->start();
    }

    ~Game() {
        voices.reset();
        rows.reset();
    }

    /* n frames of 1/60 s, as the frame hooks run them: the worker's renders
     * back, the voices' frame, the rows', then the engine's audio. */
    void frames(int n) {
        for (int i = 0; i < n; i++) {
            voices->wait_for_worker();
            voices->frame(now, false);
            rows->frame(now, launcher);
            std::vector<float> block(800 * 2);
            engine.render(block.data(), 800);
            for (float s : block) {
                loudest = std::max(loudest, std::fabs(s));
            }
            now += std::chrono::microseconds(16667);
        }
    }
    void seconds(double s) { frames(static_cast<int>(s * 60.0 + 0.5)); }

    const a11y::NamedValue& named(const char* key) const {
        const a11y::NamedValue* found = settings->model().find(key);
        assert(found != nullptr);
        return *found;
    }
    /* A row's step, as the page makes it. */
    void step(const char* key, int direction) {
        const a11y::NamedValue& value = named(key);
        if (value.kind == a11y::ValueKind::choice) {
            const std::string option =
                a11y::choice_step(value, a11y::choice_options(value), direction);
            if (!option.empty()) {
                settings->choose(value, option, now);
            }
        } else {
            settings->set(value, value.value + static_cast<float>(direction) * value.step, now);
        }
    }
    /* A hand edit of the file, polled. */
    void edit(const std::string& from, const std::string& to) {
        std::string text = read_file(path);
        assert(text.find(from) != std::string::npos);
        text.replace(text.find(from), from.size(), to);
        edit_file(path, text);
        now += a11y::ValueFile::kPollInterval * 2;
        settings->poll(now);
    }
    std::string file() const { return read_file(path); }
    /* The row's text while no voice is listed. */
    std::string none(Voice voice) const { return rows->voice(voice).named().none(); }
};

const char* const kSample = "[a11y] voice yours: \"Your voice\"";
const char* const kTheirSample = "[a11y] voice opponents: \"Opponents' voice\"";

/* In the submenu they are given, after the row that opens it, your voice's
 * four first; the values in the file under their own headings, after
 * Steering, the samples' delay under Speech's. */
void the_rows_are_in_their_submenu() {
    Game game("rows.ini");
    std::vector<std::string> labels;
    const a11y::NamedMenu* menu = nullptr;
    for (const a11y::ShownRow& row : game.settings->shown()) {
        if (row.opens != nullptr) {
            menu = row.opens;
            labels.push_back("[" + row.opens->label + "]");
            assert(row.menu == nullptr);
            continue;
        }
        labels.push_back(row.value != nullptr ? row.value->label : row.action->label);
        assert(row.menu == (labels.size() > 4 ? menu : nullptr));
    }
    assert((labels == std::vector<std::string>{"Accessibility", "Speech", "Steering",
                          "[Voice settings]", "Your voice", "Your voice rate", "Your voice volume",
                          "Preview your voice", "Opponents' voice", "Opponents' voice rate",
                          "Opponents' voice volume", "Preview opponents' voice"}));
    assert(game.named("your_voice.rate").value == 50.0f);
    assert(game.named("opponents_voice.volume").value == 100.0f);
    assert(!game.named("speech.voice_sample_delay").shown);
    assert(!game.named("your_voice.boost").shown && !game.named("opponents_voice.boost").shown);
    const std::string file = game.file();
    const size_t speech = file.find("\nspeech.on = on\n");
    const size_t delay = file.find("\nspeech.voice_sample_delay = ");
    const size_t yours = file.find("\n# Your voice\n");
    const size_t theirs = file.find("\n# Opponents' voice\n");
    const size_t steering = file.find("\nsteering.on = on\n");
    assert(speech != std::string::npos && speech < delay && delay < steering && steering < yours &&
           yours < theirs);
    assert(has_line(file, "your_voice.voice = Zira"));
    assert(has_line(file, "opponents_voice.voice = Mark"));
    assert(has_line(file, "your_voice.rate = 50"));
    assert(has_line(file, "opponents_voice.volume = 100"));
    assert(has_line(file, "your_voice.boost = 3"));
}

void languages_are_named() {
    assert(a11y::language_name("en-US") == "English (United States)");
    assert(a11y::language_name("en-GB") == "English (United Kingdom)");
    assert(a11y::language_name("fi-FI") == "Finnish (Finland)");
    assert(a11y::language_name("fi") == "Finnish");
    /* Said as written where the table lacks a part. */
    assert(a11y::language_name("en-ZZ") == "en-ZZ");
    assert(a11y::language_name("xx-YY") == "xx-YY");
    assert(a11y::language_name("") == "");
}

/* The installed voices, sorted by language and then name; the language
 * after each name only when they span more than one. */
void the_options_are_the_installed_voices() {
    Game several("several.ini");
    assert(several.none(Voice::yours) == "Starting");
    assert(several.rows->voice(Voice::yours).options().empty());
    several.frames(1);
    const a11y::ChoiceOptions options = several.rows->voice(Voice::yours).options();
    assert((options ==
            a11y::ChoiceOptions{{"Microsoft George", "Microsoft George, English (United Kingdom)"},
                {"Microsoft David", "Microsoft David, English (United States)"},
                {"Microsoft Mark", "Microsoft Mark, English (United States)"},
                {"Microsoft Zira", "Microsoft Zira, English (United States)"},
                {"Microsoft Heidi", "Microsoft Heidi, Finnish (Finland)"}}));
    assert(several.rows->voice(Voice::yours).option() == "Microsoft Zira");
    assert(several.rows->voice(Voice::opponents).option() == "Microsoft Mark");
    /* Both defaults are installed: nothing said of them. */
    assert(several.spoken.empty());

    Game one("one.ini", "",
        {{"Microsoft Zira", "en-US"}, {"Microsoft David", "en-US"}, {"Microsoft Mark", "en-US"}},
        "");
    one.frames(1);
    assert((one.rows->voice(Voice::opponents).options() ==
            a11y::ChoiceOptions{
                {"Microsoft David", ""}, {"Microsoft Mark", ""}, {"Microsoft Zira", ""}}));
    /* A step saves the display name, and the voice speaks with it. */
    one.step("your_voice.voice", 1);
    assert(one.rows->voice(Voice::yours).saved() == "Microsoft David");
    assert(has_line(one.file(), "your_voice.voice = Microsoft David"));
    assert(one.voices->voice_in_use(Voice::yours) == "Microsoft David");
    one.step("your_voice.voice", -1);
    one.step("your_voice.voice", -1);
    assert(one.voices->voice_in_use(Voice::yours) == "Microsoft Mark");
}

/* A change plays a sample in its voice a moment later; a quick run of
 * changes plays one, at the value it settles on. Preview plays at once. */
void a_change_plays_a_sample() {
    Game game("samples.ini");
    game.frames(1);
    const double delay = game.rows->sample_delay();
    assert(delay > 0.2);

    game.step("your_voice.rate", 1);
    game.seconds(delay * 0.5);
    assert(!logged(kSample));
    game.step("your_voice.rate", 1);
    game.seconds(delay * 0.5);
    game.step("your_voice.rate", 1);
    game.seconds(delay * 0.8);
    assert(!logged(kSample));
    game.seconds(delay * 0.4);
    assert(count(kSample) == 1);
    assert(logged("[a11y] voice yours plays: \"Your voice\""));
    assert(!logged(kTheirSample));
    {
        /* Rendered at the rate the run settled on. */
        std::lock_guard<std::mutex> lock(game.calls.mutex);
        assert(game.calls.set_rates.size() == 1);
        assert(game.calls.set_rates[0] == std::make_pair(0, 0.65f));
    }
    assert(has_line(game.file(), "your_voice.rate = 65"));

    /* The opponents' voice: its own voice and volume, its own sample. */
    s_lines.clear();
    game.step("opponents_voice.voice", 1);
    game.step("opponents_voice.volume", -1);
    game.seconds(delay * 1.2);
    assert(count(kTheirSample) == 1);
    assert(!logged(kSample));
    assert(game.voices->voice_in_use(Voice::opponents) == "Microsoft Zira");
    {
        std::lock_guard<std::mutex> lock(game.calls.mutex);
        /* Zira, the bridge's third. */
        assert(game.calls.set_voices.back() == std::make_pair(1, 2));
    }

    /* Preview at once, and a sample waiting goes with it. */
    s_lines.clear();
    game.step("your_voice.volume", -1);
    assert(game.rows->preview("your_voice"));
    assert(count(kSample) == 1);
    game.seconds(delay * 2.0);
    assert(count(kSample) == 1);
    assert(game.rows->preview("opponents_voice"));
    assert(count(kTheirSample) == 1);
    assert(!game.rows->preview("gap_warning"));

    /* A hand edit is a change too. */
    s_lines.clear();
    game.edit("your_voice.rate = 65", "your_voice.rate = 30");
    game.seconds(delay * 1.2);
    assert(count(kSample) == 1);
    {
        std::lock_guard<std::mutex> lock(game.calls.mutex);
        assert(game.calls.set_rates.back() == std::make_pair(0, 0.3f));
    }
}

/* The boost, a tuning value, lifts the voice under its volume, and a hand
 * edit of it plays a sample like a row's change. */
void the_boost_lifts_the_voice() {
    Game game("boost.ini", "your_voice.volume = 50\n");
    game.frames(1);
    const double delay = game.rows->sample_delay();
    assert(game.rows->preview("your_voice"));
    game.seconds(2.0);
    const float plain = game.loudest;
    assert(plain > 0.1f);

    s_lines.clear();
    game.loudest = 0.0f;
    game.edit("your_voice.boost = 3", "your_voice.boost = 1.5");
    game.seconds(delay * 1.2);
    assert(count(kSample) == 1);
    game.seconds(2.0);
    assert(std::fabs(game.loudest - 0.5f * plain) < 0.01f);
}

/* The file's values reach the voices at the start, with no sample. */
void the_saved_values_apply_at_the_start() {
    Game game("saved.ini",
        "your_voice.voice = Microsoft David\nyour_voice.rate = 80\nopponents_voice.volume = 40\n");
    game.frames(1);
    assert(game.voices->voice_in_use(Voice::yours) == "Microsoft David");
    assert(game.rows->voice(Voice::yours).option() == "Microsoft David");
    assert(logged("[a11y] voice yours speaks with Microsoft David"));
    game.seconds(game.rows->sample_delay() * 2.0);
    assert(!logged(kSample) && !logged(kTheirSample));
    game.rows->preview("your_voice");
    game.frames(1);
    std::lock_guard<std::mutex> lock(game.calls.mutex);
    assert(game.calls.set_rates.back() == std::make_pair(0, 0.8f));
}

/* A saved voice not installed: the Windows default voice speaks, the row
 * shows it, the file keeps the name, and the screen reader says so once,
 * queued. */
void a_missing_voice_falls_back() {
    Game game("missing.ini", "your_voice.voice = Hazel\n");
    assert(game.spoken.size() == 0);
    game.frames(1);
    assert((game.spoken == Spoken{{"Voice Hazel not installed, using Microsoft George.", false}}));
    assert(game.rows->voice(Voice::yours).option() == "Microsoft George");
    assert(game.rows->voice(Voice::yours).saved() == "Hazel");
    assert(game.voices->voice_in_use(Voice::yours) == "Microsoft George");
    assert(logged("[a11y] voice yours speaks with Microsoft George"));
    game.frames(30);
    assert(game.spoken.size() == 1);
    /* Other changes save around it. */
    game.step("your_voice.rate", 1);
    game.step("opponents_voice.voice", 1);
    assert(has_line(game.file(), "your_voice.voice = Hazel"));
    /* A step moves on from the voice in use, George, the first. */
    game.step("your_voice.voice", 1);
    assert(game.rows->voice(Voice::yours).saved() == "Microsoft David");
    assert(has_line(game.file(), "your_voice.voice = Microsoft David"));
    /* A hand edit naming a voice not installed is said as it is read. */
    game.edit("your_voice.voice = Microsoft David", "your_voice.voice = Susan");
    assert(
        game.spoken.back() ==
        std::make_pair(std::string("Voice Susan not installed, using Microsoft George."), false));
    assert(game.voices->voice_in_use(Voice::yours) == "Microsoft George");

    /* In the game, the news waits for the first scene's announcement, which
     * interrupts, and queues behind it. */
    Game in_game("in_game.ini", "opponents_voice.voice = Susan\n");
    in_game.launcher = false;
    in_game.frames(30);
    assert(in_game.spoken.empty());
    in_game.speech->announce("Title screen. Press Start.", a11y::Mode::interrupt);
    in_game.rows->scene_announced();
    assert((in_game.spoken == Spoken{{"Title screen. Press Start.", true},
                                  {"Voice Susan not installed, using Microsoft George.", false}}));
    in_game.rows->scene_announced();
    assert(in_game.spoken.size() == 2);

    /* Both defaults missing: one sentence for both. */
    Game bare("bare.ini", "",
        {{"Microsoft David", "en-US"}, {"Microsoft Hazel", "en-GB"}, {"Microsoft Heidi", "fi-FI"}},
        "");
    bare.frames(1);
    assert((bare.spoken ==
            Spoken{{"Voices Zira and Mark not installed, using Microsoft Hazel.", false}}));
    assert(bare.rows->voice(Voice::opponents).option() == "Microsoft Hazel");
    assert(has_line(bare.file(), "your_voice.voice = Zira"));
}

/* OneCore unavailable: the Voice rows say so and step nowhere, the samples
 * and every voice line go to the screen reader, queued, after it says once
 * that the voices are unavailable. */
void unavailable_voices_use_the_screen_reader() {
    Game game("unavailable.ini", "", a11y::test::several_languages(), "OneCore did not start");
    game.frames(1);
    assert(game.voices->state() == VoicesState::unavailable);
    assert(game.none(Voice::yours) == "Unavailable");
    assert(game.rows->voice(Voice::yours).options().empty());
    assert(game.rows->voice(Voice::yours).option().empty());
    assert((game.spoken == Spoken{{"Voices unavailable, using the screen reader.", false}}));
    game.frames(1);
    assert(game.spoken.size() == 1);
    game.step("your_voice.voice", 1);
    assert(game.rows->voice(Voice::yours).saved() == "Zira");
    game.step("your_voice.rate", 1);
    game.seconds(game.rows->sample_delay() * 1.2);
    assert(game.spoken.back() == std::make_pair(std::string("Your voice"), false));
    game.rows->preview("opponents_voice");
    assert(game.spoken.back() == std::make_pair(std::string("Opponents' voice"), false));
    assert(game.spoken.size() == 3);
}

/* Speech saved off: the voices never start, and their rows say they are
 * off. */
void speech_off_leaves_the_voices_off() {
    Game game("off.ini", "speech.on = off\n");
    game.frames(1);
    assert(game.voices->state() == VoicesState::stopped);
    assert(game.none(Voice::opponents) == "Off");
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

int main(int argc, char** argv) {
    s_dir = argc > 1 ? fs::path(argv[1]) : fs::path("voice_settings_test");
    fs::create_directories(s_dir);
    the_rows_are_in_their_submenu();
    languages_are_named();
    the_options_are_the_installed_voices();
    a_change_plays_a_sample();
    the_boost_lifts_the_voice();
    the_saved_values_apply_at_the_start();
    a_missing_voice_falls_back();
    unavailable_voices_use_the_screen_reader();
    speech_off_leaves_the_voices_off();
    std::cout << "voice_settings: all tests passed\n";
    return 0;
}
