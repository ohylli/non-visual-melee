/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select speech against a fake screen reader bridge, fed snapshots
 * of the screen written by hand. Links css_speech.cpp, css_names.cpp and
 * speech.cpp only, so this file supplies the pc_log_line that main.c normally
 * provides. */
#include "css_speech.hpp"
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

/* Characters by the decomp's numbers (character_kinds.h). */
constexpr int kDrMario = 0x16, kMario = 0x08, kLuigi = 0x07, kBowser = 0x05, kPeach = 0x0C,
              kYoshi = 0x11, kDk = 0x01, kFalcon = 0x00, kGanon = 0x19, kFalco = 0x14, kFox = 0x02,
              kNess = 0x0B, kIceClimbers = 0x0E, kKirby = 0x04, kSamus = 0x10, kZelda = 0x12,
              kLink = 0x06, kYoungLink = 0x15, kPichu = 0x18, kPikachu = 0x0D, kJigglypuff = 0x0F,
              kMewtwo = 0x0A, kGameWatch = 0x03, kMarth = 0x09, kRoy = 0x17;

/* The portraits in the game's table order (icons in mncharsel.c). */
constexpr int kPortraitCharacters[A11Y_CSS_PORTRAITS] = {kDrMario, kMario, kLuigi, kBowser, kPeach,
    kYoshi, kDk, kFalcon, kGanon, kFalco, kFox, kNess, kIceClimbers, kKirby, kSamus, kZelda, kLink,
    kYoungLink, kPichu, kPikachu, kJigglypuff, kMewtwo, kGameWatch, kMarth, kRoy};
constexpr int kFoxPortrait = 10;
constexpr int kNessPortrait = 11;
constexpr int kYoshiPortrait = 5;
constexpr int kMarthPortrait = 23;
/* What a slot's portrait holds from the moment its coin is picked up. */
constexpr int kPlaceholder = 0xD;

/* VS mode, player 1 at home with no character and a free hand, player 2 a
 * CPU with Yoshi, the others closed; every portrait unlocked. */
A11yCssState vs_screen() {
    A11yCssState state{};
    state.hands = A11Y_CSS_SLOTS;
    state.local_slot = 0;
    state.local_player = 0;
    state.hand = A11yCssHand{true, -1, -31.0f, -21.5f};
    for (A11yCssSlot& slot : state.slots) {
        slot = A11yCssSlot{A11Y_CSS_CLOSED, -1, -1, -1, 0};
    }
    state.slots[0].kind = A11Y_CSS_HUMAN;
    state.slots[1] = A11yCssSlot{A11Y_CSS_CPU, kYoshiPortrait, kYoshiPortrait, kYoshi, 0};
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        state.portraits[i].character = kPortraitCharacters[i];
    }
    return state;
}

/* Player 1's slot with its coin resting on a chosen portrait. */
void choose(A11yCssState& state, int portrait) {
    state.slots[0].portrait = portrait;
    state.slots[0].over_portrait = portrait;
    state.slots[0].character = kPortraitCharacters[portrait];
}

/* The hand has picked up the coin of a slot. */
void pick_up(A11yCssState& state, int slot) {
    state.hand.coin = slot;
    state.slots[slot].portrait = kPlaceholder;
}

/* The carried coin of a slot is over a portrait, or over none (-1). */
void carry_over(A11yCssState& state, int slot, int portrait) {
    if (portrait >= 0) {
        state.slots[slot].portrait = portrait;
    }
    state.slots[slot].over_portrait = portrait;
}

/* Speech over a fake bridge, and character select speech over that; s_lines
 * only ever holds the lines of the test that is running. */
struct Fixture {
    std::vector<Output> outputs;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(outputs)};
    a11y::CssSpeech css{speech};

    Fixture() {
        speech.init();
        s_lines.clear();
    }

    /* Arrives on the screen and drops the opening. */
    explicit Fixture(const A11yCssState& arrival) : Fixture() {
        css.frame(arrival);
        spoken();
        s_lines.clear();
    }

    void frame(const A11yCssState& state) { css.frame(state); }

    /* What was spoken since the last call. */
    std::vector<std::string> spoken() {
        std::vector<std::string> texts;
        for (const Output& output : outputs) {
            assert(output.interrupt);
            texts.push_back(output.text);
        }
        outputs.clear();
        return texts;
    }
};

using Texts = std::vector<std::string>;

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

void opening_without_a_character() {
    Fixture f;
    f.frame(vs_screen());
    assert((f.spoken() == Texts{"Character select. Player 1, no character."}));
}

void opening_with_a_character_after_a_match() {
    Fixture f;
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    f.frame(state);
    assert((f.spoken() == Texts{"Character select. Player 1, Fox."}));
}

void opening_ignores_a_stale_character() {
    /* On first arrival every slot is closed, and a character can linger
     * without a coin on its portrait. */
    Fixture f;
    A11yCssState state = vs_screen();
    state.slots[0].kind = A11Y_CSS_CLOSED;
    state.slots[0].character = kMarth;
    f.frame(state);
    state.slots[0].kind = A11Y_CSS_HUMAN;
    f.css.forget();
    f.frame(state);
    assert((f.spoken() == Texts{"Character select. Player 1, no character.",
                              "Character select. Player 1, no character."}));
}

void opening_names_the_online_player() {
    Fixture f;
    A11yCssState state = vs_screen();
    state.local_slot = 1;
    state.local_player = 1;
    f.frame(state);
    assert((f.spoken() == Texts{"Character select. Player 2, Yoshi."}));
}

void unchanged_snapshot_speaks_nothing() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    f.frame(state);
    f.frame(state);
    assert(f.spoken().empty());
}

void coin_jumps_in_enters_portraits_and_leaves_all() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    /* The hand reaches the portraits and the coin jumps into it, over no
     * portrait yet. */
    state.hand.y = 1.0f;
    pick_up(state, 0);
    f.frame(state);
    f.frame(state);
    carry_over(state, 0, kFoxPortrait);
    f.frame(state);
    carry_over(state, 0, kNessPortrait);
    f.frame(state);
    carry_over(state, 0, -1);
    f.frame(state);
    assert((f.spoken() == Texts{"Holding your coin", "Fox", "Ness"}));
}

void coin_leaving_and_entering_the_same_portrait_names_it_again() {
    A11yCssState state = vs_screen();
    pick_up(state, 0);
    carry_over(state, 0, kFoxPortrait);
    Fixture f(state);
    carry_over(state, 0, -1);
    f.frame(state);
    carry_over(state, 0, kFoxPortrait);
    f.frame(state);
    assert((f.spoken() == Texts{"Fox"}));
}

void locked_portrait_under_the_coin_says_nothing() {
    A11yCssState state = vs_screen();
    state.portraits[kNessPortrait].locked = true;
    pick_up(state, 0);
    carry_over(state, 0, kFoxPortrait);
    Fixture f(state);
    /* The game never hovers a locked portrait: over it, the coin is over
     * none. */
    state.hand.x += 7.0f;
    carry_over(state, 0, -1);
    f.frame(state);
    assert(f.spoken().empty());
}

void choice_says_nothing() {
    A11yCssState state = vs_screen();
    pick_up(state, 0);
    carry_over(state, 0, kFoxPortrait);
    Fixture f(state);
    state.hand.coin = -1;
    choose(state, kFoxPortrait);
    f.frame(state);
    assert(f.spoken().empty());
}

void choice_of_the_character_held_before_says_nothing() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    pick_up(state, 0);
    carry_over(state, 0, kFoxPortrait);
    Fixture f(state);
    state.hand.coin = -1;
    choose(state, kFoxPortrait);
    f.frame(state);
    assert(f.spoken().empty());
}

void picking_up_ones_own_coin() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    Fixture f(state);
    /* A near the resting coin: the portrait under it stays Fox. The frame of
     * the pickup is silent, the next says where the coin is. */
    pick_up(state, 0);
    f.frame(state);
    assert(f.spoken().empty());
    f.frame(state);
    f.frame(state);
    assert((f.spoken() == Texts{"Holding your coin. Fox"}));
}

void b_calls_the_coin_back_onto_another_portrait() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    Fixture f(state);
    /* The hand is over Ness when B calls the coin back from Fox: the first
     * frame still shows Fox, the next Ness. */
    pick_up(state, 0);
    f.frame(state);
    carry_over(state, 0, kNessPortrait);
    f.frame(state);
    f.frame(state);
    assert((f.spoken() == Texts{"Holding your coin. Ness"}));
}

void picking_up_a_cpu_coin() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    pick_up(state, 1);
    f.frame(state);
    f.frame(state);
    carry_over(state, 1, kFoxPortrait);
    f.frame(state);
    assert((f.spoken() == Texts{"Holding player 2's coin. Yoshi", "Fox"}));
}

void carrying_the_coin_down_clears_the_character() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    pick_up(state, 0);
    carry_over(state, 0, -1);
    Fixture f(state);
    state.hand.coin = -1;
    state.slots[0] = A11yCssSlot{A11Y_CSS_HUMAN, -1, -1, -1, 0};
    f.frame(state);
    assert((f.spoken() == Texts{"No character"}));
}

void b_puts_the_coin_back_from_another_portrait() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    pick_up(state, 0);
    carry_over(state, 0, kMarthPortrait);
    Fixture f(state);
    state.hand.coin = -1;
    choose(state, kFoxPortrait);
    f.frame(state);
    assert((f.spoken() == Texts{"Back to Fox"}));
}

void b_puts_the_coin_back_from_empty_space() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    pick_up(state, 0);
    carry_over(state, 0, kMarthPortrait);
    carry_over(state, 0, -1);
    Fixture f(state);
    state.hand.coin = -1;
    choose(state, kFoxPortrait);
    f.frame(state);
    assert((f.spoken() == Texts{"Back to Fox"}));
}

void costume_changes() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    Fixture f(state);
    state.slots[0].costume = 1;
    f.frame(state);
    state.slots[0].costume = 0;
    f.frame(state);
    assert((f.spoken() == Texts{"Costume 2", "Costume 1"}));
}

void costume_of_a_carried_coin_changes() {
    A11yCssState state = vs_screen();
    pick_up(state, 1);
    carry_over(state, 1, kFoxPortrait);
    Fixture f(state);
    state.slots[1].costume = 3;
    f.frame(state);
    assert((f.spoken() == Texts{"Costume 4"}));
}

void entering_a_portrait_resets_the_costume_unsaid() {
    A11yCssState state = vs_screen();
    pick_up(state, 0);
    carry_over(state, 0, kFoxPortrait);
    state.slots[0].costume = 2;
    Fixture f(state);
    carry_over(state, 0, kNessPortrait);
    state.slots[0].costume = 0;
    f.frame(state);
    assert((f.spoken() == Texts{"Ness"}));
}

void free_hand_over_portraits_says_nothing() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    Fixture f(state);
    for (float y = -21.5f; y < 20.0f; y += 2.0f) {
        state.hand.y = y;
        f.frame(state);
    }
    assert(f.spoken().empty());
}

void another_players_coin_moving_says_nothing() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    /* Player 3's hand carries the CPU's coin about and changes its
     * costume. */
    pick_up(state, 1);
    state.hand.coin = -1;
    f.frame(state);
    carry_over(state, 1, kFoxPortrait);
    f.frame(state);
    state.slots[1].costume = 2;
    f.frame(state);
    carry_over(state, 1, kNessPortrait);
    f.frame(state);
    assert(f.spoken().empty());
}

void missing_name_is_spoken_by_number_and_logged_once() {
    A11yCssState state = vs_screen();
    state.portraits[kFoxPortrait].character = 27;
    pick_up(state, 0);
    Fixture f(state);
    carry_over(state, 0, kFoxPortrait);
    f.frame(state);
    carry_over(state, 0, -1);
    f.frame(state);
    carry_over(state, 0, kFoxPortrait);
    f.frame(state);
    assert((f.spoken() == Texts{"Unknown character 27", "Unknown character 27"}));
    assert(count_logged("[a11y] character 27: not in the character names table") == 1);
}

void single_player_modes_say_only_the_opening() {
    Fixture f;
    A11yCssState state = vs_screen();
    state.hands = 1;
    state.local_player = 2;
    f.frame(state);
    pick_up(state, 0);
    f.frame(state);
    carry_over(state, 0, kFoxPortrait);
    f.frame(state);
    assert((f.spoken() == Texts{"Character select. Player 3, no character."}));
}

void leaving_screen_says_nothing() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    state.leaving = true;
    pick_up(state, 0);
    f.frame(state);
    assert(f.spoken().empty());
}

void forgetting_makes_the_next_frame_an_arrival() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    f.css.forget();
    f.frame(state);
    assert((f.spoken() == Texts{"Character select. Player 1, no character."}));
}

void every_portrait_has_a_name() {
    A11yCssState state = vs_screen();
    pick_up(state, 0);
    Fixture f(state);
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        carry_over(state, 0, i);
        f.frame(state);
    }
    assert((f.spoken() == Texts{"Dr. Mario", "Mario", "Luigi", "Bowser", "Peach", "Yoshi", "DK",
                              "C. Falcon", "Ganondorf", "Falco", "Fox", "Ness", "Ice Climbers",
                              "Kirby", "Samus", "Zelda", "Link", "Young Link", "Pichu", "Pikachu",
                              "Jigglypuff", "Mewtwo", "Mr. Game & Watch", "Marth", "Roy"}));
    assert(s_lines.size() == A11Y_CSS_PORTRAITS); /* the speak lines alone */
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
    opening_without_a_character();
    opening_with_a_character_after_a_match();
    opening_ignores_a_stale_character();
    opening_names_the_online_player();
    unchanged_snapshot_speaks_nothing();
    coin_jumps_in_enters_portraits_and_leaves_all();
    coin_leaving_and_entering_the_same_portrait_names_it_again();
    locked_portrait_under_the_coin_says_nothing();
    choice_says_nothing();
    choice_of_the_character_held_before_says_nothing();
    picking_up_ones_own_coin();
    b_calls_the_coin_back_onto_another_portrait();
    picking_up_a_cpu_coin();
    carrying_the_coin_down_clears_the_character();
    b_puts_the_coin_back_from_another_portrait();
    b_puts_the_coin_back_from_empty_space();
    costume_changes();
    costume_of_a_carried_coin_changes();
    entering_a_portrait_resets_the_costume_unsaid();
    free_hand_over_portraits_says_nothing();
    another_players_coin_moving_says_nothing();
    missing_name_is_spoken_by_number_and_logged_once();
    single_player_modes_say_only_the_opening();
    leaving_screen_says_nothing();
    forgetting_makes_the_next_frame_an_arrival();
    every_portrait_has_a_name();
    std::cout << "css_speech: all tests passed\n";
    return 0;
}
