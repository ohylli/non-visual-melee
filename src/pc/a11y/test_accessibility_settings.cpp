/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The accessibility settings: the value model, its text, its file against
 * real files in a scratch folder, the line edit a menu's change saves, the
 * menus' rows, the cues' values feeding the cue engine rendered into a
 * buffer, and the choice kind of value. Speech goes to a fake screen reader bridge; this file
 * supplies the pc_log_line that main.c normally provides. Its one argument is the scratch folder.
 */
#include "accessibility_settings.hpp"
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
        file = std::make_unique<ValueFile>(
            model, speech, a11y::AccessibilitySettings::kName, path.string(), true);
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
    a11y::CueGroup gap =
        f.cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.5f);
    std::vector<const a11y::NamedValue*> changed = f.load();
    assert(changed.empty());
    assert(fs::exists(f.path));
    assert(count_logged(
               "[a11y] accessibility settings: written with defaults: " + f.path.string()) == 1);
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
    again_cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.5f);
    a11y::TextRead read = again.read_text(text);
    assert(read.changed.empty() && read.missing.empty() && read.bad_lines.empty());
}

void the_text_is_parsed_with_spaces_comments_units_and_commas() {
    ValueModel model;
    CueSettings cues(model);
    a11y::CueGroup gap =
        cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
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
    assert(
        count_logged(
            "[a11y] accessibility settings: line 1: cues.volume 150 is out of range, using 100%") ==
        1);
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
    assert((f.spoken ==
            std::vector<std::string>{"Accessibility settings, lines 2 and 4 not understood."}));
    assert(count_logged("[a11y] accessibility settings: line 2 not understood: cues.volume = loud; "
                        "cues.volume stays 50%") == 1);
    assert(count_logged("[a11y] accessibility settings: line 4 not understood: nonsense") == 1);
    /* Said once: an unchanged file is not read again. */
    f.poll_later();
    assert(f.spoken.size() == 1);
    edit_file(f.path, "cues.volume = 70\ncues.on = maybe\n");
    f.poll_later();
    assert(f.spoken.back() == "Accessibility settings, line 2 not understood.");
    assert(f.value("cues.volume") == 70.0f && f.value("cues.on") == 0.0f);
    assert(f.file->problem() == "Accessibility settings, line 2 not understood.");
    edit_file(f.path, "cues.volume = 70\ncues.on = on\n");
    f.poll_later();
    assert(f.file->problem().empty() && f.spoken.size() == 2);
}

void a_reload_applies_only_what_changed() {
    Fixture f("reload.ini");
    a11y::CueGroup gap =
        f.cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
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
    assert((s_lines == std::vector<std::string>{"[a11y] accessibility settings: gap.volume = 30"}));
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
    assert(
        count_logged(
            "[a11y] accessibility settings: old.thing is not a known key; kept in the file") == 1);
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
    a11y::CueGroup gap =
        f.cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 2.0f);
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
    assert(
        count_logged(
            "[a11y] accessibility settings: added what the file lacked, at the defaults: gap.on, "
            "gap.volume, gap.boost, gap.hold") == 1);
    /* The appended file is what was read: no reload follows. */
    assert(f.poll_later().empty());
}

void the_model_describes_each_value_for_a_menu() {
    ValueModel model;
    CueSettings cues(model);
    a11y::CueGroup gap =
        cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
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
    assert(gap.values.cue == 0 &&
           cues.add_cue("ledge", "Ledge warning", "Turns the ledge warning on or off.", 1.0f)
                   .values.cue == 1);
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
    a11y::CueGroup gap =
        f.cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    a11y::CueGroup ledge =
        f.cues.add_cue("ledge", "Ledge warning", "Turns the ledge warning on or off.", 1.0f);
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

void a_menu_change_edits_only_its_own_line() {
    Fixture f("line_edit.ini");
    a11y::CueGroup gap =
        f.cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    /* The player's own order, spacing, comments and a key the game does
     * not know; every value present, so loading appends nothing. */
    const std::string before = "# my own notes\n"
                               "cues.on = on\n"
                               "\n"
                               "  gap.volume=40   # quieter, by ear\n"
                               "old.thing = 3 s\n"
                               "cues.volume = 70\n"
                               "gap.on = off\n"
                               "gap.boost = 1\n";
    write_file(f.path, before);
    f.load();
    s_lines.clear();
    std::vector<const a11y::NamedValue*> changed =
        f.file->set(gap.values.volume.named(), 55.0f, f.now);
    assert(changed.size() == 1 && changed[0] == &gap.values.volume.named());
    assert(gap.values.volume.value() == 55.0f);
    std::string after = before;
    after.replace(after.find("gap.volume=40"), 13, "gap.volume=55");
    assert(read_file(f.path) == after);
    assert((s_lines ==
            std::vector<std::string>{"[a11y] accessibility settings: saved gap.volume = 55"}));
    /* A switch is written as the file writes one. */
    f.file->set(gap.values.on.named(), 1.0f, f.now);
    after.replace(after.find("gap.on = off"), 12, "gap.on = on");
    assert(read_file(f.path) == after);
    /* Held to the range, as the model holds it. */
    f.file->set(gap.values.volume.named(), 400.0f, f.now);
    assert(read_file(f.path).find("  gap.volume=100   # quieter, by ear\n") != std::string::npos);
    /* No change, no write. */
    s_lines.clear();
    assert(f.file->set(gap.values.volume.named(), 100.0f, f.now).empty());
    assert(s_lines.empty());
    /* The temporary file is gone after the rename. */
    assert(!fs::exists(fs::path(f.path.string() + ".tmp")));
    assert(f.spoken.empty());
}

void a_line_edit_keeps_line_endings_and_edits_the_line_read() {
    ValueModel model;
    CueSettings cues(model);
    a11y::CueGroup gap =
        cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    const a11y::NamedValue& volume = gap.values.volume.named();
    model.set(volume, 30.0f);
    assert(model.edit_text("gap.volume = 5\r\ngap.on = on\r\n", volume) ==
           "gap.volume = 30\r\ngap.on = on\r\n");
    /* Of two lines with the key, the last is the one read. */
    assert(model.edit_text("gap.volume = 1\ngap.volume = 2\n", volume) ==
           "gap.volume = 1\ngap.volume = 30\n");
    /* No value written yet, and a value right before a comment. */
    assert(model.edit_text("gap.volume =\n", volume) == "gap.volume = 30\n");
    assert(model.edit_text("gap.volume = # by ear\n", volume) == "gap.volume = 30 # by ear\n");
    assert(model.edit_text("gap.volume = 7", volume) == "gap.volume = 30");
}

void a_missing_key_is_added_under_its_group() {
    ValueModel model;
    CueSettings cues(model);
    a11y::CueGroup gap =
        cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    cues.add_cue("ledge", "Ledge warning", "Turns the ledge warning on or off.", 1.0f);
    const a11y::NamedValue& volume = gap.values.volume.named();
    model.set(volume, 30.0f);
    const std::string entry = "\n# The loudness of the gap warning, under the cue volume. 0 to "
                              "100%, default 100%.\ngap.volume = 30\n";
    /* At the end of its group, before the blank lines and the next
     * group's heading; a key commented out is no line of its own. */
    const std::string head = "# All cues\ncues.on = on\n\n# Gap warning\ngap.on = on\n"
                             "# gap.volume = 5\ngap.boost = 1\n";
    const std::string tail = "\n\n# Ledge warning\nledge.on = on\n";
    assert(model.edit_text(head + tail, volume) == head + entry + tail);
    /* The group last, its last line without an ending. */
    assert(model.edit_text("# Gap warning\ngap.on = on", volume) ==
           "# Gap warning\ngap.on = on\n" + entry);
    /* No heading: the group's heading with the line, at the end. */
    assert(model.edit_text("cues.on = on\n", volume) == "cues.on = on\n\n# Gap warning\n" + entry);
    assert(model.edit_text("", volume) == "\n# Gap warning\n" + entry);
    /* A text with \r\n gets \r\n. */
    assert(model.edit_text("# Gap warning\r\ngap.on = on\r\n", volume) ==
           "# Gap warning\r\ngap.on = on\r\n\r\n# The loudness of the gap warning, under the cue "
           "volume. 0 to 100%, default 100%.\r\ngap.volume = 30\r\n");

    /* In the file: the player deletes the line, the poll puts the value back
     * to its default, and a menu's change adds the line under its group. */
    Fixture f("under_group.ini");
    a11y::CueGroup file_gap =
        f.cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    f.cues.add_cue("ledge", "Ledge warning", "Turns the ledge warning on or off.", 1.0f);
    write_file(f.path, head + tail +
                           "ledge.volume = 100\nledge.boost = 1\ncues.volume = 100\n"
                           "gap.volume = 60\n");
    f.load();
    assert(file_gap.values.volume.value() == 60.0f);
    const std::string edited = head + tail +
                               "ledge.volume = 100\nledge.boost = 1\n"
                               "cues.volume = 100\n";
    edit_file(f.path, edited);
    f.poll_later();
    assert(file_gap.values.volume.value() == 100.0f);
    f.file->set(file_gap.values.volume.named(), 30.0f, f.now);
    const std::string saved = read_file(f.path);
    assert(
        saved == head + entry + tail + "ledge.volume = 100\nledge.boost = 1\ncues.volume = 100\n");
}

void the_games_own_write_is_polled_back_silently() {
    Fixture f("own_write.ini");
    a11y::CueGroup gap =
        f.cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    f.load();
    s_lines.clear();
    f.now += ValueFile::kPollInterval * 3;
    f.file->set(gap.values.volume.named(), 30.0f, f.now);
    assert(s_lines.size() == 1);
    /* The poll finds the file as the game saved it: nothing read, applied,
     * logged or said, however often it looks. */
    assert(f.poll_later().empty());
    assert(f.poll_later().empty());
    assert(f.file->poll(f.now + std::chrono::seconds(10)).empty());
    assert(s_lines.size() == 1 && f.spoken.empty());
    assert(gap.values.volume.value() == 30.0f);
    /* And the file holds the change for the next start. */
    ValueModel again;
    CueSettings again_cues(again);
    again_cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    again.read_text(read_file(f.path));
    assert(again.find("gap.volume")->value == 30.0f);
}

void a_hand_edit_not_yet_polled_survives_a_menu_change() {
    Fixture f("hand_and_menu.ini");
    a11y::CueGroup gap =
        f.cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    f.load();
    std::string text = read_file(f.path);
    text.replace(text.find("cues.volume = 100"), 17, "cues.volume = 40");
    edit_file(f.path, text);
    /* At once, before the poll would look again. */
    std::vector<const a11y::NamedValue*> changed = f.file->set(gap.values.on.named(), 0.0f, f.now);
    assert(changed.size() == 2);
    assert(f.value("cues.volume") == 40.0f && !gap.values.on.on());
    const std::string saved = read_file(f.path);
    assert(saved.find("\ncues.volume = 40\n") != std::string::npos);
    assert(saved.find("\ngap.on = off\n") != std::string::npos);
    assert(count_logged("[a11y] accessibility settings: cues.volume = 40") == 1);
}

std::string row_name(const a11y::ShownRow& row) {
    return row.value != nullptr ? row.value->label : "[" + row.action->label + "]";
}

void the_shown_rows_list_in_order_with_each_cues_three() {
    ValueModel model;
    CueSettings cues(model);
    a11y::CueGroup gap =
        cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    a11y::Number hold = gap.group.add_number(
        a11y::NumberSpec{"hold", "Hold", "Ticks of hold.", "ticks", 4.0f, 0.0f, 30.0f, 1.0f});
    a11y::Switch wide =
        cues.add_shared("both", "Both cues")
            .add_switch(a11y::SwitchSpec{"wide", "Wide pan", "Pans wide.", true, true});
    a11y::CueGroup ledge =
        cues.add_cue("ledge", "Ledge warning", "Turns the ledge warning on or off.", 1.0f);
    a11y::Number distance = ledge.group.add_number(a11y::NumberSpec{"distance",
        "Ledge warning distance", "How far.", "units", 30.0f, 5.0f, 100.0f, 1.0f, true});
    std::vector<std::string> names;
    for (const a11y::ShownRow& row : model.shown()) {
        assert((row.value == nullptr) != (row.action == nullptr));
        names.push_back(row_name(row));
    }
    assert((
        names == std::vector<std::string>{"Cues", "Cue volume", "Gap warning", "Gap warning volume",
                     "[Preview gap warning]", "Wide pan", "Ledge warning", "Ledge warning volume",
                     "[Preview ledge warning]", "Ledge warning distance"}));
    const std::vector<a11y::ShownRow>& rows = model.shown();
    assert(rows[2].value == &gap.values.on.named() && rows[3].value == &gap.values.volume.named());
    assert(rows[4].action->group == "gap" && rows[4].action->key == "gap.preview");
    assert(rows[4].action->description == "Plays a short sample of the gap warning, as it sounds "
                                          "in a match.");
    assert(rows[5].value == &wide.named() && rows[8].action->group == "ledge");
    assert(rows[9].value == &distance.named());
    /* The rest are tuning values: in the file, not in the menus. */
    assert(!gap.values.boost.named().shown && !hold.named().shown);
    assert(model.find("gap.preview") == nullptr);
    const std::string text = model.to_text();
    assert(text.find("gap.hold = 4\n") != std::string::npos);
    assert(text.find("preview") == std::string::npos && text.find("Preview") == std::string::npos);
}

void the_settings_tell_their_listener_every_change() {
    std::vector<std::string> spoken;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(spoken)};
    speech.init();
    const fs::path path = s_dir / "a11y.ini";
    fs::remove(path);
    s_lines.clear();
    a11y::AccessibilitySettings settings(speech, path.string(), true);
    CueSettings cues(settings.model());
    a11y::CueGroup gap =
        cues.add_cue("gap", "Gap warning", "Turns the gap warning on or off.", 1.0f);
    std::vector<std::string> heard;
    auto listener = [&heard](const std::vector<const a11y::NamedValue*>& changed) {
        for (const a11y::NamedValue* named : changed) {
            heard.push_back(named->key + " = " + a11y::format_value(*named));
        }
    };
    settings.listen(listener);
    Clock::time_point now = Clock::now();
    settings.load(now);
    assert(heard.empty() && fs::exists(path));
    assert(read_file(path).rfind("# Accessibility settings for Non-Visual Melee.\n", 0) == 0);
    assert(settings.shown().size() == 5);
    /* A menu's change. */
    assert(settings.set(gap.values.volume.named(), 20.0f, now));
    assert((heard == std::vector<std::string>{"gap.volume = 20"}));
    assert(!settings.set(gap.values.volume.named(), 20.0f, now));
    assert(heard.size() == 1);
    /* A hand edit. */
    std::string text = read_file(path);
    text.replace(text.find("cues.on = on"), 12, "cues.on = off");
    edit_file(path, text);
    settings.poll(now + ValueFile::kPollInterval * 2);
    assert((heard == std::vector<std::string>{"gap.volume = 20", "cues.on = off"}));
    assert(settings.problem().empty() && settings.problems_spoken() == 0);

    /* No file: changes apply and are told, and nothing is saved. */
    a11y::AccessibilitySettings bare(speech, "", true);
    CueSettings bare_cues(bare.model());
    heard.clear();
    bare.listen(listener);
    bare.load(now);
    bare.poll(now + std::chrono::seconds(5));
    assert(bare.set(*bare.model().find("cues.volume"), 50.0f, now));
    assert((heard == std::vector<std::string>{"cues.volume = 50"}));
    assert(
        count_logged("[a11y] accessibility settings: cues.volume = 50, not saved: no file") == 1);
    assert(bare.problem().empty() && spoken.empty());
}

/* An environment variable's override holds its value for the run and is
 * never written: not when the file is first written, not when a hand edit
 * is read, not when another value is saved. A hand edit of the overridden
 * key waits under it; a menu's change lifts it and is saved. */
void an_override_holds_for_the_run_and_is_never_written() {
    std::vector<std::string> spoken;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(spoken)};
    speech.init();
    const fs::path path = s_dir / "override.ini";
    fs::remove(path);
    s_lines.clear();
    a11y::AccessibilitySettings settings(speech, path.string(), true);
    CueSettings cues(settings.model());
    std::vector<std::string> heard;
    settings.listen([&heard](const std::vector<const a11y::NamedValue*>& changed) {
        for (const a11y::NamedValue* named : changed) {
            heard.push_back(named->key + " = " + a11y::format_value(*named));
        }
    });
    const a11y::NamedValue& on = *settings.model().find("cues.on");
    assert(settings.override_value("cues.on", 0.0f, "MELEE_A11Y_CUES=0"));
    assert(!settings.override_value("cues.nothing", 0.0f, "nothing"));
    assert(count_logged("[a11y] accessibility settings: cues.on = off for this run "
                        "(MELEE_A11Y_CUES=0), not saved") == 1);
    Clock::time_point now = Clock::now();
    settings.load(now);
    assert(on.value == 0.0f && on.overridden && heard.empty());
    assert(read_file(path).find("cues.on = on\n") != std::string::npos);
    assert(!cues.mix_level().on);

    /* Another value saved: the overridden line stays as the file had it. */
    assert(settings.set(*settings.model().find("cues.volume"), 40.0f, now));
    std::string text = read_file(path);
    assert(text.find("cues.on = on\n") != std::string::npos);
    assert(text.find("cues.volume = 40\n") != std::string::npos);

    /* A hand edit elsewhere is read; the override holds. */
    text.replace(text.find("cues.volume = 40"), 16, "cues.volume = 30");
    edit_file(path, text);
    now += ValueFile::kPollInterval * 2;
    settings.poll(now);
    assert((heard == std::vector<std::string>{"cues.volume = 40", "cues.volume = 30"}));
    assert(on.value == 0.0f);
    /* A hand edit of the overridden key waits under it, logged. */
    text = read_file(path);
    text.replace(text.find("cues.on = on"), 12, "cues.on = off");
    edit_file(path, text);
    now += ValueFile::kPollInterval * 2;
    settings.poll(now);
    assert(heard.size() == 2 && on.value == 0.0f && on.saved == 0.0f);
    text = read_file(path);
    text.replace(text.find("cues.on = off"), 13, "cues.on = on");
    edit_file(path, text);
    now += ValueFile::kPollInterval * 2;
    settings.poll(now);
    assert(heard.size() == 2 && on.value == 0.0f && on.saved == 1.0f);
    assert(count_logged("[a11y] accessibility settings: cues.on = on in the file, held at off "
                        "for this run") == 1);

    /* A menu's change lifts it, and is saved; the file leads again. */
    assert(settings.set(on, 1.0f, now));
    assert(!on.overridden && on.value == 1.0f && heard.back() == "cues.on = on");
    assert(count_logged("[a11y] accessibility settings: saved cues.on = on") == 1);
    assert(read_file(path).find("cues.on = on\n") != std::string::npos);
    text = read_file(path);
    text.replace(text.find("cues.on = on"), 12, "cues.on = off");
    edit_file(path, text);
    now += ValueFile::kPollInterval * 2;
    settings.poll(now);
    assert(on.value == 0.0f && heard.back() == "cues.on = off");

    /* An override to the value the file has, then a menu's change back to
     * it: the file's value differs from neither, so nothing to save. */
    ValueModel model;
    a11y::Switch flag = model.group("x", "X").add_switch(a11y::SwitchSpec{"on", "X", "", true});
    assert(!model.override_value(flag.named(), 1.0f));
    assert(flag.named().overridden && !model.set(flag.named(), 1.0f));
    assert(!flag.named().overridden);
    /* Overridden off over a file at on: setting off keeps the game's value
     * and still counts as a change, since the file holds on. */
    model.override_value(flag.named(), 0.0f);
    assert(model.set(flag.named(), 0.0f) && flag.named().value == 0.0f);
    assert(model.to_text().find("x.on = off\n") != std::string::npos);
    assert(spoken.empty());
}

/* A file that lacks an overridden key, as after a hand edit removed its
 * line: the line appended is the default the file now holds, never the
 * override, so the next run without the variable is as the file says. */
void an_override_of_a_key_the_file_lacks_is_not_appended() {
    Fixture f("override_missing.ini");
    write_file(f.path, "cues.volume = 60\n");
    const a11y::NamedValue& on = *f.model.find("cues.on");
    f.model.override_value(on, 0.0f);
    f.load();
    assert(on.value == 0.0f && on.overridden && on.saved == 1.0f);
    const std::string text = read_file(f.path);
    assert(text.find("cues.on = on\n") != std::string::npos);
    assert(text.find("cues.on = off") == std::string::npos);
    assert(f.poll_later().empty());
}

/* The match announcements' kind of choice: fixed options, saved as the
 * option's text, which the file describes; a front end's choice edits its
 * line alone. */
a11y::Choice add_damage(ValueModel& model) {
    return model.group("damage", "Damage")
        .add_choice(a11y::ChoiceSpec{
            .name = "yours",
            .label = "Your damage",
            .description = "When your damage is said.",
            .default_option = "Every change",
            .options = {{"Off", ""}, {"Every change", ""}, {"Thresholds", ""}},
            .shown = true,
        });
}

void a_choice_is_saved_as_its_options_text() {
    Fixture f("choice_text.ini");
    a11y::Choice damage = add_damage(f.model);
    assert(damage.named().kind == a11y::ValueKind::choice);
    assert(f.load().empty());
    assert(damage.option() == "Every change" && damage.saved() == "Every change");
    std::string before = read_file(f.path);
    assert(before.find("# When your damage is said. Off, Every change or Thresholds, default "
                       "Every change.\ndamage.yours = Every change\n") != std::string::npos);
    s_lines.clear();
    std::vector<const a11y::NamedValue*> changed =
        f.file->choose(damage.named(), "Thresholds", f.now);
    assert(changed.size() == 1 && changed[0] == &damage.named());
    assert(damage.option() == "Thresholds");
    std::string after = before;
    after.replace(after.find("damage.yours = Every change"), 27, "damage.yours = Thresholds");
    assert(read_file(f.path) == after);
    assert(
        (s_lines == std::vector<std::string>{"[a11y] accessibility settings: saved damage.yours = "
                                             "Thresholds"}));
    /* The same choice again: nothing to save. */
    assert(f.file->choose(damage.named(), "Thresholds", f.now).empty());
    /* A choice is not a number, and a number not a choice. */
    assert(f.file->set(damage.named(), 1.0f, f.now).empty());
    assert(!f.model.choose(*f.model.find("cues.volume"), "Off"));
    /* Read back at the next start. */
    ValueModel again;
    CueSettings again_cues(again);
    a11y::Choice again_damage = add_damage(again);
    a11y::TextRead read = again.read_text(after);
    assert(read.bad_lines.empty() && read.missing.empty());
    assert(read.changed.size() == 1 && again_damage.option() == "Thresholds");
    assert(f.spoken.empty());
}

/* A hand edit: the line's text, read as written; an option's text in any
 * case names it. An empty value is a line not understood. */
void a_hand_edit_of_a_choice_is_picked_up() {
    Fixture f("choice_hand.ini");
    a11y::Choice damage = add_damage(f.model);
    f.load();
    std::string text = read_file(f.path);
    text.replace(text.find("damage.yours = Every change"), 27, "damage.yours =  off  # quiet");
    edit_file(f.path, text);
    std::vector<const a11y::NamedValue*> changed = f.poll_later();
    assert(changed.size() == 1 && changed[0] == &damage.named());
    assert(damage.saved() == "off" && damage.option() == "Off");
    assert(count_logged("[a11y] accessibility settings: damage.yours = off") == 1);
    /* A menu's choice keeps the comment. */
    f.file->choose(damage.named(), "Every change", f.now);
    assert(
        read_file(f.path).find("\ndamage.yours =  Every change  # quiet\n") != std::string::npos);
    text = read_file(f.path);
    text.replace(text.find("damage.yours =  Every change"), 28, "damage.yours =");
    edit_file(f.path, text);
    assert(f.poll_later().empty());
    assert(damage.saved() == "Every change");
    assert(f.spoken.size() == 1 && f.spoken[0].find("not understood") != std::string::npos);
    assert(count_logged_containing("damage.yours stays Every change") == 1);
}

/* A saved text that names no option stays in the file, unchanged by every
 * other save, while the option in use is the fallback; picking an option
 * replaces it. */
void a_saved_option_not_listed_is_kept_and_the_fallback_used() {
    Fixture f("choice_unlisted.ini");
    a11y::Choice damage = add_damage(f.model);
    f.load();
    std::string text = read_file(f.path);
    text.replace(text.find("damage.yours = Every change"), 27, "damage.yours = Sometimes");
    edit_file(f.path, text);
    f.poll_later();
    assert(damage.saved() == "Sometimes" && damage.option() == "Every change");
    assert(count_logged_containing(
               "damage.yours = Sometimes is none of the options; using Every change") == 1);
    f.file->set(*f.model.find("cues.volume"), 30.0f, f.now);
    assert(read_file(f.path).find("\ndamage.yours = Sometimes\n") != std::string::npos);
    assert(f.poll_later().empty() && damage.saved() == "Sometimes");
    /* A step goes from the option in use. */
    assert(a11y::choice_step(damage.named(), damage.options(), 1) == "Thresholds");
    f.file->choose(damage.named(), "Thresholds", f.now);
    assert(read_file(f.path).find("\ndamage.yours = Thresholds\n") != std::string::npos);

    /* A fallback of the registration's own. */
    ValueModel model;
    a11y::Choice picky = model.group("x", "X").add_choice(a11y::ChoiceSpec{
        .name = "pick",
        .default_option = "B",
        .options = {{"A", ""}, {"B", ""}, {"C", ""}},
        .fallback = [](std::string_view, const a11y::ChoiceOptions&) { return std::string("C"); },
    });
    assert(picky.option() == "B");
    model.read_text("x.pick = Z\n");
    assert(picky.saved() == "Z" && picky.option() == "C");
    /* A fallback naming no option: the default's, then the first. */
    a11y::Choice lost = model.group("x", "X").add_choice(a11y::ChoiceSpec{
        .name = "lost",
        .default_option = "B",
        .options = {{"A", ""}, {"B", ""}},
        .fallback = [](std::string_view, const a11y::ChoiceOptions&) { return std::string("Q"); },
    });
    model.read_text("x.lost = Z\n");
    assert(lost.option() == "B");
    a11y::Choice first = model.group("x", "X").add_choice(a11y::ChoiceSpec{
        .name = "first",
        .default_option = "Q",
        .options = {{"A", ""}, {"B", ""}},
    });
    assert(first.option() == "A");
    /* No override for a choice. */
    assert(!model.override_value(lost.named(), 0.0f) && !lost.named().overridden);
}

/* The Voice rows' kind of choice: options asked at run time, which change
 * between reads; the saved text is matched against them as they stand. */
void a_choices_options_may_change_between_reads() {
    Fixture f("choice_list.ini");
    a11y::ChoiceOptions voices{{"Microsoft David", ""}, {"Microsoft Mark", ""},
        {"Microsoft Zira", "Microsoft Zira, English (United States)"}};
    int asked = 0;
    auto list = [&] {
        ++asked;
        return voices;
    };
    /* Your voice's default, as the spec has it: the first voice whose name
     * holds "Zira", else the Windows default voice. */
    auto fallback = [](std::string_view saved, const a11y::ChoiceOptions& options) {
        for (const a11y::ChoiceOption& option : options) {
            if (option.text.find(saved) != std::string::npos) {
                return option.text;
            }
        }
        return std::string("Microsoft David");
    };
    a11y::Choice voice = f.model.group("voices", "Voices")
                             .add_choice(a11y::ChoiceSpec{
                                 .name = "yours",
                                 .label = "Your voice",
                                 .description = "The voice that speaks what happens to you.",
                                 .default_option = "Zira",
                                 .list = list,
                                 .fallback = fallback,
                                 .shown = true,
                             });
    f.load();
    /* The file does not depend on what is installed when it is written. */
    assert(asked == 0);
    assert(read_file(f.path).find("# The voice that speaks what happens to you. One of the "
                                  "options the menus list, default Zira.\nvoices.yours = Zira\n") !=
           std::string::npos);
    assert(voice.saved() == "Zira" && voice.option() == "Microsoft Zira");
    assert(a11y::choice_index(voice.named(), voice.options()) == 2);
    assert(voice.options()[2].shown() == "Microsoft Zira, English (United States)");
    assert(voice.options()[1].shown() == "Microsoft Mark");
    f.file->choose(voice.named(), "Microsoft Mark", f.now);
    assert(voice.option() == "Microsoft Mark");
    /* Mark uninstalled: the fallback, the file unchanged. */
    voices.erase(voices.begin() + 1);
    assert(voice.option() == "Microsoft David" && voice.saved() == "Microsoft Mark");
    assert(f.poll_later().empty());
    assert(read_file(f.path).find("\nvoices.yours = Microsoft Mark\n") != std::string::npos);
    /* Reinstalled, at another place in the list: Mark again. */
    voices.push_back({"Microsoft Mark", ""});
    assert(voice.option() == "Microsoft Mark");
    assert(a11y::choice_index(voice.named(), voice.options()) == 2);
    /* None listed. */
    voices.clear();
    assert(voice.option().empty() && a11y::choice_index(voice.named(), voice.options()) == -1);
    assert(a11y::choice_step(voice.named(), voice.options(), 1).empty());
}

/* A step wraps round at either end, as the base port's choice rows do. */
void a_choice_step_wraps_at_each_end() {
    ValueModel model;
    a11y::Choice damage = add_damage(model);
    const a11y::ChoiceOptions options = damage.options();
    model.choose(damage.named(), "Thresholds");
    assert(a11y::choice_step(damage.named(), options, 1) == "Off");
    assert(a11y::choice_step(damage.named(), options, -1) == "Every change");
    model.choose(damage.named(), "Off");
    assert(a11y::choice_step(damage.named(), options, -1) == "Thresholds");
    assert(a11y::choice_step(damage.named(), options, 1) == "Every change");
    /* One option: a step stays on it. */
    a11y::Choice one = model.group("x", "X").add_choice(
        a11y::ChoiceSpec{.name = "one", .default_option = "A", .options = {{"A", ""}}});
    assert(a11y::choice_step(one.named(), one.options(), 1) == "A");
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
    s_dir = argc > 1 ? fs::path(argv[1]) : fs::path("accessibility_settings_test");
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
    a_menu_change_edits_only_its_own_line();
    a_line_edit_keeps_line_endings_and_edits_the_line_read();
    a_missing_key_is_added_under_its_group();
    the_games_own_write_is_polled_back_silently();
    a_hand_edit_not_yet_polled_survives_a_menu_change();
    the_shown_rows_list_in_order_with_each_cues_three();
    the_settings_tell_their_listener_every_change();
    an_override_holds_for_the_run_and_is_never_written();
    an_override_of_a_key_the_file_lacks_is_not_appended();
    a_choice_is_saved_as_its_options_text();
    a_hand_edit_of_a_choice_is_picked_up();
    a_saved_option_not_listed_is_kept_and_the_fallback_used();
    a_choices_options_may_change_between_reads();
    a_choice_step_wraps_at_each_end();
    std::cout << "accessibility_settings: all tests passed\n";
    return 0;
}
