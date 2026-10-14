/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The value model, its text, its file against real files in a
 * scratch folder, and the cue settings feeding the cue engine rendered into
 * a buffer. Speech goes to a fake screen reader bridge; this file supplies
 * the pc_log_line that main.c normally provides. Its one argument is the
 * scratch folder. */
#include "cue_engine.hpp"
#include "cue_settings.hpp"
#include "value_model.hpp"
#include "value_file.hpp"
#include "speech.hpp"
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
using a11y::CueSettings;
using a11y::ValueFile;
using a11y::ValueModel;

namespace fs = std::filesystem;

std::vector<std::string> s_lines;
fs::path s_dir;

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    explicit FakeBridge(std::vector<std::string>& spoken) : m_spoken(spoken) {}

    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view utf8, bool) override {
        m_spoken.emplace_back(utf8);
        return "";
    }

private:
    std::vector<std::string>& m_spoken;
};

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

std::size_t count_logged_containing(const std::string& part) {
    return static_cast<std::size_t>(std::count_if(s_lines.begin(), s_lines.end(),
        [&](const std::string& line) { return line.find(part) != std::string::npos; }));
}

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

/* The model with the cue values, its file in a fresh scratch file,
 * and speech over a fake bridge. Each test starts with an empty log. */
struct Fixture {
    std::vector<std::string> spoken;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(spoken)};
    ValueModel model;
    CueSettings cues{model};
    fs::path path;
    std::unique_ptr<ValueFile> file;
    Clock::time_point now = Clock::now();

    explicit Fixture(const char* name) : path(s_dir / name) {
        speech.init();
        fs::remove(path);
        s_lines.clear();
    }

    void open() {
        file = std::make_unique<ValueFile>(model, speech, "Cue settings", path.string(), true);
    }

    std::vector<const a11y::NamedValue*> load() {
        open();
        return file->load(now);
    }

    std::vector<const a11y::NamedValue*> poll_later() {
        now += ValueFile::kPollInterval;
        return file->poll(now);
    }

    float value(const char* key) const { return model.find(key)->value; }
};

void a_missing_file_is_written_with_defaults_and_descriptions() {
    Fixture f("defaults.ini");
    a11y::CueGroup gap = f.cues.add_cue("gap", "Gap warning", 1.5f);
    std::vector<const a11y::NamedValue*> changed = f.load();
    assert(changed.empty());
    assert(fs::exists(f.path));
    assert(count_logged("[a11y] cue settings: written with defaults: " + f.path.string()) == 1);
    const std::string text = read_file(f.path);
    for (const char* expected :
        {
            "# All cues\n",
            "# Turns every cue on or off. On or off, default on.\ncues.on = on\n",
            "default 100%.\ncues.volume = 100\n",
            "# Gap warning\n",
            "# Turns the gap warning on or off. On or off, default on.\ngap.on = on\n",
            "0 to 100%, default 100%.\ngap.volume = 100\n",
            "1 to 8 times, default 1.5 times.\ngap.boost = 1.5\n",
        })
    {
        assert(text.find(expected) != std::string::npos);
    }
    assert(!gap.values.boost.named().description.empty());
    /* Every value line has its description right above it. */
    std::size_t at = 0;
    while ((at = text.find(" = ", at)) != std::string::npos) {
        std::size_t line = text.rfind('\n', at);
        std::size_t above = text.rfind('\n', line - 1);
        assert(text[above + 1] == '#');
        at += 3;
    }
    /* Read back, the file gives every default. */
    ValueModel again;
    CueSettings again_cues(again);
    again_cues.add_cue("gap", "Gap warning", 1.5f);
    a11y::TextRead read = again.read_text(text);
    assert(read.changed.empty() && read.missing.empty() && read.bad_lines.empty());
}

void the_text_is_parsed_with_spaces_comments_units_and_commas() {
    ValueModel model;
    CueSettings cues(model);
    a11y::CueGroup gap = cues.add_cue("gap", "Gap warning", 1.0f);
    a11y::TextRead read = model.read_text("\xEF\xBB\xBF# a comment\r\n"
                                          "\r\n"
                                          "  cues.on=OFF   \r\n"
                                          "cues.volume = 80% # quieter\r\n"
                                          "gap.volume=+40 %\r\n"
                                          "gap.boost = 2,5\r\n"
                                          "gap.on = no");
    assert(read.bad_lines.empty() && read.notes.empty());
    assert(!model.find("cues.on")->value);
    assert(model.find("cues.volume")->value == 80.0f);
    assert(gap.values.volume.value() == 40.0f);
    assert(gap.values.boost.value() == 2.5f);
    assert(!gap.values.on.on());
    assert(read.changed.size() == 5);
    a11y::CueLevel level = gap.values.level();
    assert(!level.on && std::fabs(level.volume - 0.4f) < 1e-6f && level.boost == 2.5f);
    assert(cues.mix_level().volume == 0.8f && !cues.mix_level().on);
    assert(a11y::format_value(*model.find("gap.boost")) == "2.5");
    assert(a11y::format_value(*model.find("gap.on")) == "off");
}

void a_value_out_of_range_is_clamped_and_logged() {
    Fixture f("clamped.ini");
    write_file(f.path, "cues.volume = 150\ncues.on = on\n");
    std::vector<const a11y::NamedValue*> changed = f.load();
    assert(f.value("cues.volume") == 100.0f);
    assert(changed.empty());
    assert(count_logged(
               "[a11y] cue settings: line 1: cues.volume 150 is out of range, using 100%") == 1);
    write_file(f.path, "cues.volume = -3\n");
    f.file->load(f.now);
    assert(f.value("cues.volume") == 0.0f);
    assert(f.spoken.empty());
}

void an_unreadable_line_keeps_its_last_value_and_is_reported_by_number() {
    Fixture f("bad.ini");
    write_file(f.path, "cues.volume = 50\ncues.on = on\n");
    f.load();
    assert(f.value("cues.volume") == 50.0f);
    edit_file(f.path, "# tuning\ncues.volume = loud\ncues.on = off\nnonsense\n");
    std::vector<const a11y::NamedValue*> changed = f.poll_later();
    assert(f.value("cues.volume") == 50.0f);
    assert(f.value("cues.on") == 0.0f);
    assert(changed.size() == 1 && changed[0]->key == "cues.on");
    assert((f.spoken == std::vector<std::string>{"Cue settings, lines 2 and 4 not understood."}));
    assert(count_logged("[a11y] cue settings: line 2 not understood: cues.volume = loud; "
                        "cues.volume stays 50%") == 1);
    assert(count_logged("[a11y] cue settings: line 4 not understood: nonsense") == 1);
    /* Said once: an unchanged file is not read again. */
    f.poll_later();
    assert(f.spoken.size() == 1);
    edit_file(f.path, "cues.volume = 70\ncues.on = maybe\n");
    f.poll_later();
    assert(f.spoken.back() == "Cue settings, line 2 not understood.");
    assert(f.value("cues.volume") == 70.0f && f.value("cues.on") == 0.0f);
    assert(f.file->problem() == "Cue settings, line 2 not understood.");
    edit_file(f.path, "cues.volume = 70\ncues.on = on\n");
    f.poll_later();
    assert(f.file->problem().empty() && f.spoken.size() == 2);
}

void a_reload_applies_only_what_changed() {
    Fixture f("reload.ini");
    a11y::CueGroup gap = f.cues.add_cue("gap", "Gap warning", 1.0f);
    f.load();
    const std::string text = read_file(f.path);
    s_lines.clear();
    /* Not looked at again before the interval. */
    std::string edited = text;
    edited.replace(edited.find("gap.volume = 100"), 16, "gap.volume = 30");
    edit_file(f.path, edited);
    assert(f.file->poll(f.now + std::chrono::milliseconds(100)).empty());
    assert(gap.values.volume.value() == 100.0f);
    std::vector<const a11y::NamedValue*> changed = f.poll_later();
    assert(changed.size() == 1 && changed[0] == &gap.values.volume.named());
    assert(gap.values.volume.value() == 30.0f);
    assert((s_lines == std::vector<std::string>{"[a11y] cue settings: gap.volume = 30"}));
    /* Unchanged since: nothing read, nothing applied. */
    assert(f.poll_later().empty());
    /* Rewritten with the same text: read, nothing applied. */
    edit_file(f.path, edited);
    assert(f.poll_later().empty());
    assert(s_lines.size() == 1);
}

void a_deleted_file_is_written_again_and_its_defaults_apply() {
    Fixture f("deleted.ini");
    write_file(f.path, "cues.volume = 20\n");
    f.load();
    assert(f.value("cues.volume") == 20.0f);
    fs::remove(f.path);
    std::vector<const a11y::NamedValue*> changed = f.poll_later();
    assert(fs::exists(f.path));
    assert(f.value("cues.volume") == 100.0f);
    assert(changed.size() == 1);
}

void an_unknown_key_is_logged_once_and_kept() {
    Fixture f("unknown.ini");
    write_file(f.path, "cues.on = on\ncues.volume = 100\nold.thing = 3 s\n");
    f.load();
    assert(count_logged(
               "[a11y] cue settings: old.thing is not a known setting; kept in the file") == 1);
    assert(f.spoken.empty());
    assert(f.model.unknown().at("old.thing") == "3 s");
    edit_file(f.path, "cues.on = off\ncues.volume = 100\nold.thing = 3 s\n");
    f.poll_later();
    assert(count_logged_containing("old.thing") == 1);
    assert(read_file(f.path).find("old.thing = 3 s\n") != std::string::npos);
    /* A front end rewriting the whole file keeps it too. */
    assert(f.model.to_text().find("\nold.thing = 3 s\n") != std::string::npos);
}

void a_newly_registered_value_takes_its_default_and_joins_the_file() {
    Fixture f("newer.ini");
    write_file(f.path, "# tuned by hand\ncues.on = on\ncues.volume = 60\n");
    a11y::CueGroup gap = f.cues.add_cue("gap", "Gap warning", 2.0f);
    a11y::Number hold = gap.group.add_number(a11y::NumberSpec{"hold", "Gap hold",
        "Ticks the warning holds on after a floor appears.", "ticks", 4.0f, 0.0f, 30.0f, 1.0f});
    f.load();
    assert(f.value("cues.volume") == 60.0f);
    assert(gap.values.on.on() && gap.values.volume.value() == 100.0f);
    assert(gap.values.boost.value() == 2.0f && hold.value() == 4.0f);
    const std::string text = read_file(f.path);
    assert(text.rfind("# tuned by hand\ncues.on = on\ncues.volume = 60\n", 0) == 0);
    assert(text.find("\n# Gap warning\n") != std::string::npos);
    assert(text.find("0 to 30 ticks, default 4 ticks.\ngap.hold = 4\n") != std::string::npos);
    assert(text.find("cues.volume = 100") == std::string::npos);
    assert(count_logged("[a11y] cue settings: added what the file lacked, at the defaults: gap.on, "
                        "gap.volume, gap.boost, gap.hold") == 1);
    /* The appended file is what was read: no reload follows. */
    assert(f.poll_later().empty());
}

void the_model_describes_each_value_for_a_menu() {
    ValueModel model;
    CueSettings cues(model);
    a11y::CueGroup gap = cues.add_cue("gap", "Gap warning", 1.0f);
    assert(model.groups().size() == 2);
    assert(model.groups()[0].title == "All cues" && model.groups()[0].values.size() == 2);
    assert(model.groups()[1].title == "Gap warning");
    const a11y::NamedValue& volume = gap.values.volume.named();
    assert(volume.kind == a11y::ValueKind::number && volume.label == "Gap warning volume");
    assert(volume.min == 0.0f && volume.max == 100.0f && volume.step > 0.0f && volume.unit == "%");
    assert(gap.values.on.named().kind == a11y::ValueKind::toggle);
    /* A front end's change is held to the range. */
    assert(model.set(volume, 20.0f) && !model.set(volume, 20.0f));
    assert(model.set(volume, 140.0f) && volume.value == 100.0f);
    assert(!model.set(volume, 120.0f));
    assert(gap.values.cue == 0 && cues.add_cue("ledge", "Ledge warning", 1.0f).values.cue == 1);
}

/* Interleaved stereo, rendered in blocks the size of a WASAPI callback. */
float rms(a11y::CueEngine& engine, int frames) {
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int done = 0; done < frames; done += 480) {
        engine.render(
            out.data() + static_cast<std::size_t>(done) * 2, std::min(480, frames - done));
    }
    /* The last tenth, after every glide. */
    double sum = 0.0;
    std::size_t from = out.size() - out.size() / 10;
    for (std::size_t i = from; i < out.size(); ++i) {
        sum += static_cast<double>(out[i]) * out[i];
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(out.size() - from)));
}

void cue_settings_feed_the_volume_chain() {
    Fixture f("engine.ini");
    a11y::CueGroup gap = f.cues.add_cue("gap", "Gap warning", 1.0f);
    a11y::CueGroup ledge = f.cues.add_cue("ledge", "Ledge warning", 1.0f);
    a11y::CueEngine engine({false, "", ""});
    a11y::VoiceSpec spec;
    spec.name = "gap";
    spec.cue = gap.values.cue;
    spec.sound.samples.assign(a11y::kCueRate / 10, 0.25f);
    int voice = engine.add_voice(spec);
    assert(voice >= 0);
    engine.start(voice, a11y::VoiceTargets{});
    f.load();
    f.cues.apply(engine);
    const float full = rms(engine, a11y::kCueRate / 10);
    assert(full > 0.01f);

    /* Cue volume halves it; the ledge warning's values leave it alone. */
    edit_file(f.path, "cues.volume = 50\nledge.volume = 10\n");
    std::vector<const a11y::NamedValue*> changed = f.poll_later();
    assert(changed.size() == 2);
    f.cues.apply(engine, changed);
    assert(std::fabs(rms(engine, a11y::kCueRate / 10) - full / 2) < full * 0.01f);
    assert(ledge.values.volume.value() == 10.0f);

    /* Its own switch silences it, the voice still running underneath. */
    edit_file(f.path, "cues.volume = 50\nledge.volume = 10\ngap.on = off\n");
    f.cues.apply(engine, f.poll_later());
    assert(rms(engine, a11y::kCueRate / 10) < full * 0.001f);
    assert(engine.playing(voice));

    /* Back on, then the all-cues switch off. */
    edit_file(f.path, "cues.volume = 50\nledge.volume = 10\ncues.on = off\n");
    f.cues.apply(engine, f.poll_later());
    assert(rms(engine, a11y::kCueRate / 10) < full * 0.001f);
    edit_file(f.path, "cues.volume = 100\ngap.boost = 2\n");
    f.cues.apply(engine, f.poll_later());
    assert(std::fabs(rms(engine, a11y::kCueRate / 10) - full * 2) < full * 0.05f);
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
    s_dir = argc > 1 ? fs::path(argv[1]) : fs::path("cue_settings_test");
    fs::create_directories(s_dir);
    a_missing_file_is_written_with_defaults_and_descriptions();
    the_text_is_parsed_with_spaces_comments_units_and_commas();
    a_value_out_of_range_is_clamped_and_logged();
    an_unreadable_line_keeps_its_last_value_and_is_reported_by_number();
    a_reload_applies_only_what_changed();
    a_deleted_file_is_written_again_and_its_defaults_apply();
    an_unknown_key_is_logged_once_and_kept();
    a_newly_registered_value_takes_its_default_and_joins_the_file();
    the_model_describes_each_value_for_a_menu();
    cue_settings_feed_the_volume_chain();
    std::cout << "cue_settings: all tests passed\n";
    return 0;
}
