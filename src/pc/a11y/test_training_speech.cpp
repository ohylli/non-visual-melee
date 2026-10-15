/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Training menu speech against hand-written snapshots of the menu and a fake
 * clock, over a fake screen reader bridge. Links training_speech.cpp,
 * training_names.cpp, item_names.cpp and speech.cpp only, so this file
 * supplies the pc_log_line that main.c normally provides. */
#include "item_names.hpp"
#include "speech.hpp"
#include "training_names.hpp"
#include "training_speech.hpp"
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

using Outputs = std::vector<Output>;

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    explicit FakeBridge(Outputs& outputs) : m_outputs(outputs) {}

    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view utf8, bool interrupt) override {
        m_outputs.push_back(Output{std::string(utf8), interrupt});
        return "";
    }

private:
    Outputs& m_outputs;
};

constexpr const char* kHint =
    "Up and down for rows, left and right to adjust, A on Item, Reset and Finish, Start to "
    "close.";

/* The menu as the scene's start leaves it: the cursor on Speed, x1. */
struct Menu {
    bool open = false;
    int row = A11Y_TRAINING_SPEED;
    int values[A11Y_TRAINING_VALUE_SLOTS] = {2, 0, 0, 0, 0, 0, 0};

    A11yTrainingMenu snapshot() const {
        A11yTrainingMenu menu{};
        menu.open = open;
        menu.row = row;
        menu.value = row >= 0 && row < A11Y_TRAINING_VALUE_SLOTS ? values[row] : 0;
        return menu;
    }
};

struct Fixture {
    Outputs outputs;
    a11y::Speech speech{a11y::Config{true, false}, std::make_unique<FakeBridge>(outputs)};
    a11y::TrainingSpeech training{speech};
    Menu menu;
    /* The fake clock: a tick is a sixtieth of a second. */
    static constexpr std::chrono::microseconds kTick{16667};
    /* The ticks after a change at which the settle has passed. */
    static constexpr int kSettleTicks = static_cast<int>(
        (std::chrono::duration_cast<std::chrono::microseconds>(a11y::TrainingSpeech::kSettle) +
            kTick - std::chrono::microseconds(1)) /
        kTick);
    a11y::Clock::time_point now = a11y::Clock::time_point{} + std::chrono::hours(1);

    Fixture() {
        speech.init();
        training.session_started();
    }

    /* Runs n ticks of the menu as it stands and returns what they said,
     * clearing it. */
    Outputs ticks(int n = 1) {
        for (int i = 0; i < n; i++) {
            now += kTick;
            training.tick(menu.snapshot(), now);
        }
        Outputs said = std::move(outputs);
        outputs.clear();
        return said;
    }

    Outputs open() {
        menu.open = true;
        return ticks();
    }

    Outputs close() {
        menu.open = false;
        return ticks();
    }

    /* The cursor moves as the game's Up and Down do, skipping the hidden row
     * and wrapping. */
    Outputs down() {
        menu.row = menu.row == A11Y_TRAINING_FINISH ? A11Y_TRAINING_SPEED : menu.row + 1;
        if (menu.row == A11Y_TRAINING_HIDDEN) {
            menu.row = A11Y_TRAINING_CAMERA;
        }
        return ticks();
    }

    Outputs up() {
        menu.row = menu.row == A11Y_TRAINING_SPEED ? A11Y_TRAINING_FINISH : menu.row - 1;
        if (menu.row == A11Y_TRAINING_HIDDEN) {
            menu.row = A11Y_TRAINING_CPU_DAMAGE;
        }
        return ticks();
    }

    Outputs set(int value) {
        menu.values[menu.row] = value;
        return ticks();
    }

    /* Ticks enough for any held change to be said. */
    Outputs settle() { return ticks(30); }
};

Outputs interrupt(const std::string& text) {
    return {{text, true}};
}

Outputs queued(const std::string& text) {
    return {{text, false}};
}

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

void opening_with_the_hint() {
    Fixture f;
    assert(f.ticks(10).empty());
    assert((f.open() == interrupt(std::string("Training menu. Speed: x1. ") + kHint)));
    assert(f.ticks(30).empty());
}

void hint_once_per_session() {
    Fixture f;
    f.open();
    f.ticks(30);
    assert(f.close().empty());
    f.ticks(20);
    assert((f.open() == interrupt("Training menu. Speed: x1.")));
    f.close();
    f.training.session_started();
    f.menu = Menu{};
    assert((f.open() == interrupt(std::string("Training menu. Speed: x1. ") + kHint)));
}

void opening_names_the_remembered_row() {
    Fixture f;
    f.open();
    f.ticks(30);
    for (int i = 0; i < 4; i++) {
        f.down();
        f.ticks(30);
    }
    f.set(57);
    f.ticks(30);
    assert(f.close().empty());
    f.ticks(20);
    assert((f.open() == interrupt("Training menu. CP Damage: 57.")));
}

void every_row_and_the_wrap() {
    Fixture f;
    f.open();
    f.ticks(30);
    const char* expected[] = {
        "Item: Food",
        "No. of CPs, 1",
        "CP, Stand",
        "CP Damage: 0",
        "Camera: Normal",
        "Reset",
        "Finish",
        "Speed: x1",
    };
    for (const char* text : expected) {
        assert((f.down() == interrupt(text)));
        assert(f.ticks(30).empty());
    }
    /* Up wraps from Speed to Finish, and skips the hidden row too. */
    assert((f.up() == interrupt("Finish")));
    f.ticks(30);
    f.up();
    f.ticks(30);
    f.up();
    f.ticks(30);
    assert((f.up() == interrupt("CP Damage: 0")));
}

void every_value_word() {
    Fixture f;
    f.open();
    f.ticks(30);
    /* Right goes slower on Speed, and every row wraps. */
    const char* speeds[] = {"x2", "x1.5", "x1", "2/3", "1/2", "1/4"};
    for (int i = 3; i < 9; i++) {
        assert((f.set(i % 6) == interrupt(speeds[i % 6])));
        f.ticks(30);
    }

    f.down();
    f.ticks(30);
    const char* items[] = {"Food", "Maxim Tomato", "Heart Container", "Ray Gun", "Fire Flower",
        "Super Scope", "Star Rod", "Lip's Stick", "Beam Sword", "Home-Run Bat", "Fan", "Hammer",
        "Green Shell", "Red Shell", "Flipper", "Freezie", "Mr. Saturn", "Poké Ball", "Bob-omb",
        "Motion-Sensor Bomb", "Super Mushroom", "Poison Mushroom", "Starman", "Parasol",
        "Warp Star", "Metal Box", "Bunny Hood", "Screw Attack", "Barrel Cannon", "Cloaking Device"};
    for (int i = 1; i <= 30; i++) {
        assert((f.set(i % 30) == interrupt(items[i % 30])));
        f.ticks(30);
    }

    f.down();
    f.ticks(30);
    for (int i : {1, 2, 0}) {
        assert((f.set(i) == interrupt(std::to_string(i + 1))));
        f.ticks(30);
    }

    f.down();
    f.ticks(30);
    const char* behaviours[] = {"Stand", "Walk", "Evade", "Jump", "Attack", "Human"};
    for (int i = 1; i <= 6; i++) {
        assert((f.set(i % 6) == interrupt(behaviours[i % 6])));
        f.ticks(30);
    }

    f.down();
    f.ticks(30);
    for (int i : {1, 57, 999, 0}) {
        assert((f.set(i) == interrupt(std::to_string(i))));
        f.ticks(30);
    }

    f.down();
    f.ticks(30);
    const char* cameras[] = {"Normal", "Zoom", "Free"};
    for (int i : {1, 2, 0}) {
        assert((f.set(i) == interrupt(cameras[i])));
        f.ticks(30);
    }
    assert(s_lines.empty());
}

void item_names_match_item_switch() {
    /* One table names the items on both screens. */
    for (int i = 0; i < 30; i++) {
        A11yItem item = a11y::training_item(i);
        assert(item != A11Y_NO_ITEM);
        assert(a11y::training_value_words(a11y::TrainingValue::item, i) ==
               std::string(*a11y::item_name(item)));
    }
    assert(a11y::training_item(30) == A11Y_NO_ITEM);
    assert(a11y::training_item(-1) == A11Y_NO_ITEM);
}

void hidden_row_is_never_named() {
    Fixture f;
    f.open();
    f.ticks(30);
    f.menu.row = A11Y_TRAINING_HIDDEN;
    assert(f.ticks(30).empty());
    assert(count_logged("[a11y] training menu row 5: not in the training menu names table") == 1);
    f.menu.row = A11Y_TRAINING_CAMERA;
    assert((f.ticks() == interrupt("Camera: Normal")));
    assert(a11y::training_row(A11Y_TRAINING_HIDDEN) == nullptr);
}

void reset_leaves_other_rows_unsaid() {
    Fixture f;
    f.menu.values[A11Y_TRAINING_SPEED] = 4;
    f.menu.values[A11Y_TRAINING_CAMERA] = 1;
    f.menu.row = A11Y_TRAINING_RESET;
    assert((f.open() == interrupt(std::string("Training menu. Reset. ") + kHint)));
    f.ticks(30);
    /* A on Reset puts every value back and leaves the cursor there. */
    Menu reset;
    reset.open = true;
    reset.row = A11Y_TRAINING_RESET;
    f.menu = reset;
    assert(f.ticks(30).empty());
    assert((f.up() == interrupt("Camera: Normal")));
}

void slow_series_is_said_in_full() {
    Fixture f;
    f.open();
    f.ticks(30);
    f.down();
    /* Presses 0.3 s apart: each is said at once. */
    const char* items[] = {"Maxim Tomato", "Heart Container", "Ray Gun"};
    for (int i = 1; i <= 3; i++) {
        f.ticks(17);
        assert((f.set(i) == interrupt(items[i - 1])));
    }
}

void burst_says_the_first_then_where_it_settles() {
    Fixture f;
    f.open();
    f.ticks(30);
    f.down();
    f.ticks(30);
    f.down();
    f.ticks(30);
    f.down();
    f.ticks(30);
    f.down();
    f.ticks(30);
    /* CP Damage held Right, repeats every 4 ticks: the first change, then
     * nothing while they come, then the value it settles on. */
    assert((f.set(1) == interrupt("1")));
    Outputs during;
    for (int value = 2; value <= 40; value++) {
        Outputs said = f.ticks(3);
        during.insert(during.end(), said.begin(), said.end());
        said = f.set(value);
        during.insert(during.end(), said.begin(), said.end());
    }
    assert(during.empty());
    /* Quiet shorter than the settle holds it still. */
    assert(f.ticks(Fixture::kSettleTicks - 1).empty());
    assert((f.ticks() == interrupt("40")));
    assert(f.settle().empty());
}

void burst_of_rows_settles_on_a_row() {
    Fixture f;
    f.open();
    f.ticks(30);
    assert((f.down() == interrupt("Item: Food")));
    f.ticks(7);
    assert(f.down().empty());
    f.ticks(7);
    assert(f.down().empty());
    f.ticks(7);
    assert(f.down().empty());
    assert((f.settle() == interrupt("CP Damage: 0")));
}

void value_after_a_held_row_move_names_the_row() {
    /* Down then Right quickly: the row the value belongs to is said with it,
     * as the held row move it replaces was never heard. */
    Fixture f;
    f.open();
    f.ticks(30);
    assert((f.down() == interrupt("Item: Food")));
    f.ticks(3);
    assert(f.down().empty());
    f.ticks(3);
    assert(f.set(1).empty());
    assert((f.settle() == interrupt("No. of CPs, 2")));
    /* Once the row is named, its values are said alone again. */
    assert((f.set(2) == interrupt("3")));

    /* The same when the value change comes just as the held row move would
     * have been said. */
    Fixture g;
    g.open();
    g.ticks(30);
    assert((g.down() == interrupt("Item: Food")));
    g.ticks(3);
    assert(g.down().empty());
    assert(g.ticks(Fixture::kSettleTicks - 1).empty());
    assert((g.set(1) == interrupt("No. of CPs, 2")));
    assert(g.settle().empty());
}

void burst_then_pause_then_more() {
    Fixture f;
    f.open();
    f.ticks(30);
    f.down();
    f.ticks(30);
    assert((f.set(1) == interrupt("Maxim Tomato")));
    f.ticks(3);
    assert(f.set(2).empty());
    f.ticks(3);
    assert(f.set(3).empty());
    assert((f.settle() == interrupt("Ray Gun")));
    /* A new press after the quiet is said at once. */
    assert((f.set(4) == interrupt("Fire Flower")));
}

void burst_then_closing_drops_it() {
    Fixture f;
    f.open();
    f.ticks(30);
    f.down();
    f.set(1);
    f.ticks(3);
    assert(f.set(2).empty());
    assert(f.close().empty());
    assert(f.settle().empty());
    /* Reopening names the value it settled on. */
    assert((f.open() == interrupt("Training menu. Item: Heart Container.")));
}

void press_during_the_opening_follows_it() {
    /* Input reaches the menu from the tick it opens, during its slide-in: a
     * press then is said after the opening rather than cutting it off. */
    Fixture f;
    f.open();
    f.ticks(3);
    assert(f.down().empty());
    assert((f.settle() == queued("Item: Food")));
    /* Later presses interrupt as usual. */
    assert((f.down() == interrupt("No. of CPs, 1")));
}

void press_on_the_opening_tick_is_in_the_opening() {
    Fixture f;
    f.menu.open = true;
    f.menu.row = A11Y_TRAINING_ITEM;
    assert((f.ticks() == interrupt(std::string("Training menu. Item: Food. ") + kHint)));
    assert(f.settle().empty());
}

void reopening_interrupts_and_drops() {
    Fixture f;
    f.open();
    f.ticks(30);
    f.close();
    f.ticks(10);
    assert((f.open() == interrupt("Training menu. Speed: x1.")));
    f.ticks(30);
    f.down();
    f.ticks(5);
    f.down();
    f.close();
    f.ticks(10);
    assert((f.open() == interrupt("Training menu. No. of CPs, 1.")));
    assert(f.settle().empty());
}

void unknown_value_says_its_number() {
    Fixture f;
    f.open();
    f.ticks(30);
    f.down();
    f.ticks(30);
    assert((f.set(31) == interrupt("31")));
    f.ticks(30);
    f.set(0);
    f.ticks(30);
    f.set(31);
    assert(count_logged(
               "[a11y] training menu row 1: value 31 not in the training menu names table") == 1);
    f.ticks(30);
    assert((f.down() == interrupt("No. of CPs, 1")));
    f.ticks(30);
    f.menu.values[A11Y_TRAINING_CPU_COUNT] = 7;
    f.ticks(30);
    assert((f.up() == interrupt("Item: 31")));
    assert((f.ticks(30).empty()));
    assert((f.down() == interrupt("No. of CPs, 7")));
}

void nothing_while_closed() {
    Fixture f;
    f.menu.row = A11Y_TRAINING_CAMERA;
    f.menu.values[A11Y_TRAINING_CAMERA] = 2;
    assert(f.ticks(30).empty());
    f.menu.values[A11Y_TRAINING_CAMERA] = 0;
    assert(f.ticks(30).empty());
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
    opening_with_the_hint();
    hint_once_per_session();
    opening_names_the_remembered_row();
    every_row_and_the_wrap();
    every_value_word();
    item_names_match_item_switch();
    hidden_row_is_never_named();
    reset_leaves_other_rows_unsaid();
    slow_series_is_said_in_full();
    burst_says_the_first_then_where_it_settles();
    burst_of_rows_settles_on_a_row();
    value_after_a_held_row_move_names_the_row();
    burst_then_pause_then_more();
    burst_then_closing_drops_it();
    press_during_the_opening_follows_it();
    press_on_the_opening_tick_is_in_the_opening();
    reopening_interrupts_and_drops();
    unknown_value_says_its_number();
    nothing_while_closed();
    std::cout << "training_speech: all tests passed\n";
    return 0;
}
