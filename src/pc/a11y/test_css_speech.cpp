/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Character select speech against a fake screen reader bridge, fed snapshots
 * of the screen written by hand (test_css_screen.hpp). Links css_speech.cpp,
 * css_names.cpp, css_targets.cpp and speech.cpp only, so this file supplies
 * the pc_log_line that main.c normally provides. */
#include "css_names.hpp"
#include "css_speech.hpp"
#include "speech.hpp"
#include "test_css_screen.hpp"
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

using namespace css_test;

constexpr int kPichuPortrait = 18;

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
    /* The glide as CssReader would pass it: an ended one for one frame. */
    a11y::GlideStatus glide;

    Fixture() {
        speech.init();
        s_lines.clear();
    }

    /* Arrives on the screen and drops the opening. */
    explicit Fixture(const A11yCssState& arrival) : Fixture() {
        css.frame(arrival, glide);
        spoken();
        s_lines.clear();
    }

    void frame(const A11yCssState& state) {
        css.frame(state, glide);
        if (glide.phase != a11y::GlideStatus::Phase::gliding) {
            glide = a11y::GlideStatus{};
        }
    }

    void glide_to(a11y::Target destination) {
        glide = a11y::GlideStatus{a11y::GlideStatus::Phase::gliding, destination};
    }

    void glide_ended(bool failed) {
        glide.phase = failed ? a11y::GlideStatus::Phase::failed : a11y::GlideStatus::Phase::ended;
    }

    /* What was spoken since the last call, all of it interrupting. */
    std::vector<std::string> spoken() {
        std::vector<std::string> texts;
        for (const Output& output : outputs) {
            assert(output.interrupt);
            texts.push_back(output.text);
        }
        outputs.clear();
        return texts;
    }

    /* What was spoken since the last call, interrupting or queued. */
    std::vector<Output> said() {
        std::vector<Output> out = outputs;
        outputs.clear();
        return out;
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
    state.slots[0].portrait = -1;
    state.slots[0].over_portrait = -1;
    state.slots[0].character = A11Y_NO_CHARACTER;
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
    state.hand.y = 1.0f;
    Fixture f(state);
    for (float y = 1.0f; y < 20.0f; y += 2.0f) {
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
    state.portraits[kFoxPortrait].character = static_cast<A11yCharacter>(27);
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

void single_player_openings_name_the_mode() {
    Fixture f;
    A11yCssState state = classic_screen();
    f.frame(state);
    state.match_type = A11Y_REG_ADVENTURE;
    choose(state, kFoxPortrait);
    f.css.forget();
    f.frame(state);
    state.match_type = A11Y_STADIUM_HOMERUN;
    f.css.forget();
    f.frame(state);
    assert((f.spoken() == Texts{"Classic character select. Player 1, no character.",
                              "Adventure character select. Player 1, Fox.",
                              "Home-Run Contest character select. Player 1, Fox."}));
}

void training_opening_names_the_cpu_too() {
    Fixture f;
    f.frame(training_screen());
    assert((
        f.spoken() == Texts{"Training character select. Player 1, no character. CPU, Dr. Mario."}));
}

void a_mode_started_from_another_controller_cannot_be_steered() {
    /* Player 3 started it: the badge says P3, and steering reaches only
     * controller 1. */
    Fixture f;
    A11yCssState state = classic_screen();
    state.local_player = 2;
    state.local_port = 2;
    f.frame(state);
    assert((f.spoken() == Texts{"Classic character select. Player 3, no character. Steering "
                                "needs controller 1."}));
}

void every_single_player_mode_has_a_name() {
#define A11Y_CHECK_MODE_NAME(name, number) assert(a11y::mode_name(number).has_value());
    A11Y_CSS_MATCH_TYPES(A11Y_CHECK_MODE_NAME)
#undef A11Y_CHECK_MODE_NAME
    assert(!a11y::mode_name(0).has_value());
}

void single_player_coin_is_read_as_in_vs_modes() {
    A11yCssState state = classic_screen();
    Fixture f(state);
    state.hand.y = 1.0f;
    pick_up(state, 0);
    f.frame(state);
    f.frame(state);
    carry_over(state, 0, kFoxPortrait);
    f.frame(state);
    /* A chooses: the announcer names Fox, and Ready to Fight appears. */
    state.hand.coin = -1;
    state.slots[0].character = kFox;
    f.frame(state);
    state.ready = true;
    f.frame(state);
    assert((f.spoken() == Texts{"Holding your coin", "Fox", "Ready to fight. Press Start."}));
}

/* The arrows by their numbers in kArrows. */
constexpr a11y::Target kLowerArrow{a11y::TargetKind::arrow, 0};
constexpr a11y::Target kHigherArrow{a11y::TargetKind::arrow, 1};
constexpr a11y::Target kFewerArrow{a11y::TargetKind::arrow, 2};

/* The hand at an arrow, free. */
void hand_on(A11yCssState& state, const A11yCssRect& arrow) {
    state.hand.x = (arrow.left + arrow.right) / 2.0f;
    state.hand.y = (arrow.top + arrow.bottom) / 2.0f;
}

void free_hand_on_each_arrow() {
    A11yCssState state = classic_screen();
    Fixture f(state);
    hand_on(state, kDifficultyLower);
    f.frame(state);
    hand_on(state, kDifficultyHigher);
    f.frame(state);
    hand_on(state, kStocksFewer);
    f.frame(state);
    hand_on(state, kStocksMore);
    f.frame(state);
    state.hand.x = 5.0f;
    f.frame(state);
    assert((f.spoken() == Texts{"Level: Normal, lower", "Level: Normal, higher", "Stock: 3, fewer",
                              "Stock: 3, more"}));
}

void a_on_an_arrow_says_the_new_value() {
    /* The press shows on the pad the frame before the value changes. */
    A11yCssState state = classic_screen();
    hand_on(state, kDifficultyHigher);
    Fixture f(state);
    state.pressed_a = true;
    f.frame(state);
    state.pressed_a = false;
    state.arrows.difficulty.value = 3;
    f.frame(state);
    hand_on(state, kStocksMore);
    f.frame(state);
    state.pressed_a = true;
    f.frame(state);
    state.pressed_a = false;
    state.arrows.stocks.value = 4;
    f.frame(state);
    hand_on(state, kStocksFewer);
    f.frame(state);
    state.pressed_a = true;
    f.frame(state);
    state.pressed_a = false;
    state.arrows.stocks.value = 3;
    f.frame(state);
    assert((f.spoken() == Texts{"Hard", "Stock: 3, more", "4", "Stock: 4, fewer", "3"}));
}

void a_at_the_end_of_a_range_repeats_the_value() {
    /* The game does nothing; each arrow at both ends of its range. */
    struct Case {
        const A11yCssRect* arrow;
        int difficulty;
        int stocks;
        const char* said;
    };
    const Case cases[] = {
        {&kDifficultyLower, 0, 3, "Very easy"},
        {&kDifficultyHigher, 4, 3, "Very hard"},
        {&kStocksFewer, kNormal, 1, "1"},
        {&kStocksMore, kNormal, 5, "5"},
    };
    for (const Case& c : cases) {
        A11yCssState state = classic_screen();
        state.arrows.difficulty.value = c.difficulty;
        state.arrows.stocks.value = c.stocks;
        hand_on(state, *c.arrow);
        Fixture f(state);
        state.pressed_a = true;
        f.frame(state);
        state.pressed_a = false;
        f.frame(state);
        assert((f.spoken() == Texts{c.said}));
    }
    /* The other end of each, where A changes the value, waits for it. */
    for (const Case& c : cases) {
        A11yCssState state = classic_screen();
        state.arrows.difficulty.value = c.difficulty == 0 ? 4 : c.difficulty == 4 ? 0 : kNormal;
        state.arrows.stocks.value = c.stocks == 1 ? 5 : c.stocks == 5 ? 1 : 3;
        hand_on(state, *c.arrow);
        Fixture f(state);
        state.pressed_a = true;
        f.frame(state);
        assert(f.spoken().empty());
    }
}

void a_away_from_the_arrows_says_nothing_of_them() {
    A11yCssState state = classic_screen();
    state.arrows.stocks.value = 1;
    Fixture f(state);
    state.pressed_a = true;
    f.frame(state);
    assert(f.spoken().empty());
}

void arrows_not_shown_say_nothing() {
    /* Training keeps whatever values a Classic run left. */
    A11yCssState state = training_screen();
    Fixture f(state);
    state.arrows.stocks.value = 4;
    hand_on(state, kStocksMore);
    state.pressed_a = true;
    f.frame(state);
    assert(f.spoken().empty());
}

void a_step_onto_an_arrow_names_it() {
    A11yCssState state = classic_screen();
    Fixture f(state);
    f.css.step(state, kFewerArrow);
    f.css.step(state, kHigherArrow);
    assert((f.spoken() == Texts{"Stock: 3, fewer", "Level: Normal, higher"}));
}

void a_step_down_to_the_arrows_with_ones_own_coin_clears_it() {
    A11yCssState state = classic_screen();
    pick_up(state, 0);
    carry_over(state, 0, kPichuPortrait);
    Fixture f(state);
    f.css.step(state, kLowerArrow);
    assert((f.spoken() == Texts{"No character. Level: Normal, lower"}));
}

void training_cpus_coin() {
    /* A step to the CPU's portrait says whose coin rests there; picking it
     * up says so; choosing for it is left to the announcer. */
    A11yCssState state = training_screen();
    state.hand.y = 1.0f;
    pick_up(state, 0);
    choose(state, kFoxPortrait);
    state.hand.coin = -1;
    Fixture f(state);
    f.css.step(state, a11y::Target{a11y::TargetKind::portrait, 0});
    pick_up(state, 1);
    f.frame(state);
    carry_over(state, 1, 0);
    f.frame(state);
    carry_over(state, 1, kYoshiPortrait);
    f.frame(state);
    state.hand.coin = -1;
    state.slots[1].character = kYoshi;
    f.frame(state);
    assert((f.spoken() ==
            Texts{"Dr. Mario, the CPU's coin", "Holding the CPU's coin. Dr. Mario", "Yoshi"}));
}

void leaving_screen_says_nothing() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    state.exit = A11Y_CSS_TO_STAGE_SELECT;
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

/* The hand moved to a point in one frame. */
void move(A11yCssState& state, float x, float y) {
    state.hand.x = x;
    state.hand.y = y;
}

void free_hand_enters_and_leaves_each_button() {
    A11yCssState state = vs_screen();
    state.handicap_sliders = true;
    state.slots[0].handicap_knob = A11yCssKnob{true, -30.9f, -12.0f};
    Fixture f(state);
    /* Home, then the name box, player 1's handicap knob and HMN/CPU button,
     * space, player 2's button and CPU level knob, space, and the top bar. */
    move(state, -28.0f, -18.5f);
    f.frame(state);
    move(state, -30.9f, -12.0f);
    f.frame(state);
    move(state, -32.0f, -2.0f);
    f.frame(state);
    move(state, -25.0f, -2.0f);
    f.frame(state);
    move(state, -16.0f, -2.0f);
    f.frame(state);
    move(state, -15.5f, -15.0f);
    f.frame(state);
    move(state, -8.0f, -10.0f);
    f.frame(state);
    move(state, -30.0f, 24.0f);
    f.frame(state);
    move(state, 0.0f, 24.0f);
    f.frame(state);
    move(state, 20.0f, 24.0f);
    f.frame(state);
    move(state, 16.0f, 24.0f);
    f.frame(state);
    assert((f.spoken() == Texts{"Player 1 name tag", "Player 1 handicap: 9", "Player 1: human",
                              "Player 2: CPU", "Player 2 CPU level: 1", "Teams: off", "Rules",
                              "Back"}));
}

void moving_within_a_button_says_it_once() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    for (float x = -19.0f; x < -14.0f; x += 0.5f) {
        move(state, x, -2.0f);
        f.frame(state);
    }
    assert((f.spoken() == Texts{"Player 2: CPU"}));
}

void slot_kinds_in_turn() {
    A11yCssState state = vs_screen();
    move(state, 14.0f, -2.0f);
    Fixture f(state);
    /* A on player 4's button: a new CPU gets a random character. */
    state.slots[3] = cpu(3, kPichuPortrait);
    f.frame(state);
    state.slots[3].kind = A11Y_CSS_CLOSED;
    f.frame(state);
    /* Human again, with the character a player chooses still to come. */
    state.slots[3] = slot(A11Y_CSS_HUMAN, 3);
    f.frame(state);
    assert((f.spoken() ==
            Texts{"Player 4: CPU, Pichu", "Player 4: closed", "Player 4: human, no character"}));
}

void ones_own_slot_opening_as_the_hand_reaches_the_portraits_says_only_the_coin() {
    A11yCssState state = vs_screen();
    state.slots[0].kind = A11Y_CSS_CLOSED;
    Fixture f(state);
    move(state, -31.0f, 1.0f);
    state.slots[0].kind = A11Y_CSS_HUMAN;
    pick_up(state, 0);
    f.frame(state);
    f.frame(state);
    assert((f.spoken() == Texts{"Holding your coin"}));
}

void slider_grabbed_moved_and_released() {
    A11yCssState state = vs_screen();
    move(state, -15.5f, -15.12f);
    Fixture f(state);
    state.hand.slider = A11Y_CSS_CPU_LEVEL;
    state.hand.slider_slot = 1;
    state.slots[1].cpu_level_held = true;
    f.frame(state);
    for (int level = 2; level <= 4; level++) {
        state.slots[1].cpu_level = level;
        state.slots[1].cpu_level_knob = cpu_level_knob(1, level);
        move(state, state.slots[1].cpu_level_knob.x, -15.12f);
        f.frame(state);
        f.frame(state);
    }
    state.hand.slider = A11Y_CSS_NO_SLIDER;
    state.hand.slider_slot = -1;
    state.slots[1].cpu_level_held = false;
    f.frame(state);
    f.frame(state);
    assert(
        (f.spoken() == Texts{"Holding the slider", "Level 2", "Level 3", "Level 4", "Released"}));
}

void handicap_slider_says_its_value() {
    A11yCssState state = vs_screen();
    state.handicap_sliders = true;
    state.hand.slider = A11Y_CSS_HANDICAP;
    state.hand.slider_slot = 0;
    Fixture f(state);
    state.slots[0].handicap = 8;
    f.frame(state);
    assert((f.spoken() == Texts{"Handicap 8"}));
}

void team_match_teams_button_and_team_button() {
    A11yCssState state = vs_screen();
    move(state, -30.0f, 24.0f);
    Fixture f(state);
    state.teams = true;
    f.frame(state);
    move(state, -24.0f, -3.0f);
    f.frame(state);
    state.slots[0].team = 1;
    f.frame(state);
    state.slots[0].team = 2;
    f.frame(state);
    assert((f.spoken() == Texts{"Teams: on", "Player 1 team: red", "Blue team", "Green team"}));
}

void ready_to_fight_appears_and_disappears() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    Fixture f(state);
    state.ready = true;
    f.frame(state);
    f.frame(state);
    state.ready = false;
    f.frame(state);
    assert((f.spoken() == Texts{"Ready to fight. Press Start."}));
}

void ready_to_fight_follows_the_local_hands_doing() {
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    pick_up(state, 0);
    carry_over(state, 0, kMarthPortrait);
    Fixture f(state);
    state.hand.coin = -1;
    choose(state, kFoxPortrait);
    state.ready = true;
    f.frame(state);
    assert((f.spoken() == Texts{"Back to Fox. Ready to fight. Press Start."}));
}

/* Player 3 joins and chooses Fox with their own hand. */
void another_player_joins_and_chooses(Fixture& f, A11yCssState& state) {
    state.slots[2] = slot(A11Y_CSS_HUMAN, 2);
    f.frame(state);
    state.slots[2].carried = true;
    state.slots[2].portrait = kPlaceholder;
    f.frame(state);
    state.slots[2].over_portrait = kFoxPortrait;
    state.slots[2].portrait = kFoxPortrait;
    f.frame(state);
    state.slots[2].carried = false;
    state.slots[2].character = kFox;
    f.frame(state);
    state.teams = true;
    f.frame(state);
}

void another_player_offline_says_nothing() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    another_player_joins_and_chooses(f, state);
    assert(f.said().empty());
}

void another_player_online_is_queued() {
    A11yCssState state = vs_screen();
    state.online = true;
    Fixture f(state);
    another_player_joins_and_chooses(f, state);
    assert((f.said() == std::vector<Output>{{"Player 3: human, no character", false},
                            {"Player 3: Fox", false}, {"Teams: on", false}}));
}

void another_players_choice_that_makes_it_ready_is_queued_with_it() {
    A11yCssState state = vs_screen();
    state.online = true;
    state.slots[2] = slot(A11Y_CSS_HUMAN, 2);
    state.slots[2].carried = true;
    Fixture f(state);
    state.slots[2].carried = false;
    state.slots[2].portrait = kFoxPortrait;
    state.slots[2].character = kFox;
    state.ready = true;
    f.frame(state);
    assert(
        (f.said() == std::vector<Output>{{"Player 3: Fox. Ready to fight. Press Start.", false}}));
}

void choosing_for_a_cpu_says_nothing() {
    A11yCssState state = vs_screen();
    state.online = true;
    pick_up(state, 1);
    carry_over(state, 1, kFoxPortrait);
    state.slots[1].carried = true;
    Fixture f(state);
    state.hand.coin = -1;
    state.slots[1].carried = false;
    state.slots[1].character = kFox;
    f.frame(state);
    f.frame(state);
    assert(f.said().empty());
}

void rules_screen_and_name_entry_open_and_return() {
    A11yCssState state = vs_screen();
    move(state, 0.0f, 24.0f);
    Fixture f(state);
    state.exit = A11Y_CSS_TO_RULES;
    f.frame(state);
    state.exit = A11Y_CSS_AWAY;
    f.frame(state);
    /* Coming back builds the screen afresh, the hand at home. */
    state.exit = A11Y_CSS_STAYING;
    move(state, -31.0f, -21.5f);
    f.frame(state);
    state.exit = A11Y_CSS_TO_NAME_ENTRY;
    f.frame(state);
    assert((f.spoken() == Texts{"Custom Rules. No speech yet.",
                              "Character select. Player 1, no character.",
                              "Name Entry. No speech yet."}));
}

void name_tag_window_opening() {
    A11yCssState state = vs_screen();
    move(state, -28.0f, -18.5f);
    Fixture f(state);
    state.slots[0].name_tags_open = true;
    move(state, -28.0f, -10.0f);
    f.frame(state);
    move(state, -28.0f, -12.0f);
    f.frame(state);
    assert((f.spoken() == Texts{"Name tags. No speech yet."}));
}

a11y::Target portrait(int index) {
    return a11y::Target{a11y::TargetKind::portrait, index};
}

void a_step_names_where_it_goes() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    f.css.step(state, portrait(kFoxPortrait));
    f.css.step(state, a11y::Target{});
    assert((f.spoken() == Texts{"Fox", "Nothing that way"}));
}

void a_glide_crosses_portraits_in_silence_and_arrives_silently() {
    A11yCssState state = vs_screen();
    state.hand.y = 1.0f;
    pick_up(state, 0);
    carry_over(state, 0, kFoxPortrait);
    Fixture f(state);
    f.glide_to(portrait(kMarthPortrait));
    carry_over(state, 0, kNessPortrait);
    f.frame(state);
    carry_over(state, 0, -1);
    f.frame(state);
    carry_over(state, 0, kMarthPortrait);
    f.frame(state);
    f.glide_ended(false);
    f.frame(state);
    f.frame(state);
    assert(f.spoken().empty());
}

void a_glide_ending_elsewhere_names_where_the_hand_is() {
    /* The player's stick took over on the way. */
    A11yCssState state = vs_screen();
    state.hand.y = 1.0f;
    pick_up(state, 0);
    carry_over(state, 0, kFoxPortrait);
    Fixture f(state);
    f.glide_to(portrait(kMarthPortrait));
    carry_over(state, 0, kNessPortrait);
    f.frame(state);
    f.glide_ended(false);
    f.frame(state);
    f.frame(state);
    assert((f.spoken() == Texts{"Ness"}));
}

void a_failed_glide_says_so() {
    A11yCssState state = vs_screen();
    state.hand.y = 1.0f;
    pick_up(state, 0);
    carry_over(state, 0, kFoxPortrait);
    Fixture f(state);
    f.glide_to(portrait(kMarthPortrait));
    carry_over(state, 0, kNessPortrait);
    f.frame(state);
    f.glide_ended(true);
    f.frame(state);
    assert((f.spoken() == Texts{"Could not reach Marth. Ness"}));
}

void a_free_hand_gliding_over_a_button_says_nothing() {
    A11yCssState state = vs_screen();
    Fixture f(state);
    f.glide_to(portrait(kFoxPortrait));
    move(state, -16.0f, -2.0f);
    f.frame(state);
    move(state, -23.0f, 11.0f);
    f.frame(state);
    f.glide_ended(false);
    f.frame(state);
    assert(f.spoken().empty());
}

void the_coin_jumping_in_during_a_glide_waits_behind_the_step() {
    /* The step said the portrait; the pickup is queued behind it, without
     * the portrait crossed. */
    A11yCssState state = vs_screen();
    Fixture f(state);
    f.glide_to(portrait(kFoxPortrait));
    state.hand.y = 1.0f;
    pick_up(state, 0);
    f.frame(state);
    carry_over(state, 0, kPichuPortrait);
    f.frame(state);
    assert((f.said() == std::vector<Output>{Output{"Holding your coin", false}}));
}

void a_step_to_a_coin_says_whose_it_is() {
    /* Player 2's coin rests on Yoshi, player 1's own on Fox. */
    A11yCssState state = vs_screen();
    choose(state, kFoxPortrait);
    state.slots[0].coin = coin_at_rest(kFoxPortrait);
    Fixture f(state);
    f.css.step(state, portrait(kYoshiPortrait));
    f.css.step(state, portrait(kFoxPortrait));
    f.css.step(state, portrait(kNessPortrait));
    /* Carrying a coin, a coin at rest is not for picking up. */
    pick_up(state, 0);
    f.css.step(state, portrait(kYoshiPortrait));
    assert((f.spoken() == Texts{"Yoshi, player 2's coin", "Fox, your coin", "Ness", "Yoshi"}));
}

void a_step_to_the_top_bar_with_a_coin_says_it_waits() {
    A11yCssState state = vs_screen();
    state.hand.y = 19.0f;
    pick_up(state, 0);
    carry_over(state, 0, 0); /* Dr. Mario */
    Fixture f(state);
    f.css.step(state, a11y::Target{a11y::TargetKind::teams});
    f.css.step(state, a11y::Target{a11y::TargetKind::back});
    assert((f.spoken() ==
            Texts{"Teams: off, not while holding a coin", "Back, not while holding a coin"}));
}

void a_step_down_with_ones_own_coin_clears_the_character_first() {
    /* The step says the coin goes back, then the destination; the coin
     * going back on the way says nothing more, nor does the arrival. */
    A11yCssState state = vs_screen();
    state.hand.y = 5.0f;
    choose(state, kPichuPortrait);
    pick_up(state, 0);
    carry_over(state, 0, kPichuPortrait);
    Fixture f(state);
    a11y::Target button{a11y::TargetKind::slot_button, 0};
    f.css.step(state, button);
    f.glide_to(button);
    carry_over(state, 0, -1);
    f.frame(state);
    state.hand.coin = -1;
    state.hand.y = -1.0f;
    state.slots[0].portrait = -1;
    state.slots[0].character = A11Y_NO_CHARACTER;
    f.frame(state);
    move(state, -32.1f, -2.2f);
    f.frame(state);
    f.glide_ended(false);
    f.frame(state);
    f.frame(state);
    assert((f.spoken() == Texts{"No character. Player 1: human"}));
}

void a_step_down_with_a_cpus_coin_puts_it_back_first() {
    A11yCssState state = vs_screen();
    state.hand.y = 5.0f;
    pick_up(state, 1);
    carry_over(state, 1, kPichuPortrait);
    Fixture f(state);
    a11y::Target button{a11y::TargetKind::slot_button, 1};
    f.css.step(state, button);
    f.glide_to(button);
    state.hand.coin = -1;
    state.slots[1] = cpu(1, kYoshiPortrait);
    f.frame(state);
    f.glide_ended(false);
    f.frame(state);
    assert((f.spoken() == Texts{"Back to Yoshi. Player 2: CPU"}));
}

void a_drop_after_a_step_that_did_not_leave_the_portraits_is_said() {
    /* A step down said the coin goes back, then a step up turned the glide
     * before it did; B putting it back later is said. */
    A11yCssState state = vs_screen();
    state.hand.y = 5.0f;
    choose(state, kFoxPortrait);
    pick_up(state, 0);
    carry_over(state, 0, kPichuPortrait);
    Fixture f(state);
    f.css.step(state, a11y::Target{a11y::TargetKind::slot_button, 0});
    f.glide_to(a11y::Target{a11y::TargetKind::slot_button, 0});
    f.frame(state);
    f.css.step(state, portrait(kPichuPortrait));
    f.glide_to(portrait(kPichuPortrait));
    f.frame(state);
    f.glide_ended(false);
    f.frame(state);
    f.frame(state);
    state.hand.coin = -1;
    choose(state, kFoxPortrait);
    f.frame(state);
    assert((f.spoken() == Texts{"No character. Player 1: human", "Pichu", "Back to Fox"}));
}

/* The hand holds player 2's CPU level slider at a level. */
void hold_cpu_level(A11yCssState& state, int level) {
    state.hand.slider = A11Y_CSS_CPU_LEVEL;
    state.hand.slider_slot = 1;
    state.slots[1].cpu_level_held = true;
    state.slots[1].cpu_level = level;
    state.slots[1].cpu_level_knob = cpu_level_knob(1, level);
    move(state, state.slots[1].cpu_level_knob.x, state.slots[1].cpu_level_knob.y);
}

a11y::Target level(int value) {
    return a11y::Target{a11y::TargetKind::cpu_level, 1, value};
}

void slider_steps_say_the_value_at_the_press_only() {
    A11yCssState state = vs_screen();
    hold_cpu_level(state, 3);
    Fixture f(state);
    /* Two steps right; the value passes 4 on its way to 5. */
    f.css.step(state, level(4));
    f.glide_to(level(4));
    f.css.step(state, level(5));
    f.glide_to(level(5));
    hold_cpu_level(state, 4);
    f.frame(state);
    hold_cpu_level(state, 5);
    f.frame(state);
    f.glide_ended(false);
    f.frame(state);
    f.frame(state);
    /* Up keeps the value and says it again. */
    f.css.step(state, level(5));
    assert((f.spoken() == Texts{"Level 4", "Level 5", "Level 5"}));
}

void a_slider_glide_ending_on_another_value_says_it() {
    A11yCssState state = vs_screen();
    hold_cpu_level(state, 3);
    Fixture f(state);
    f.glide_to(level(5));
    hold_cpu_level(state, 4);
    f.frame(state);
    f.glide_ended(false);
    f.frame(state);
    assert((f.spoken() == Texts{"Level 4"}));
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
    single_player_openings_name_the_mode();
    training_opening_names_the_cpu_too();
    a_mode_started_from_another_controller_cannot_be_steered();
    every_single_player_mode_has_a_name();
    single_player_coin_is_read_as_in_vs_modes();
    free_hand_on_each_arrow();
    a_on_an_arrow_says_the_new_value();
    a_at_the_end_of_a_range_repeats_the_value();
    a_away_from_the_arrows_says_nothing_of_them();
    arrows_not_shown_say_nothing();
    a_step_onto_an_arrow_names_it();
    a_step_down_to_the_arrows_with_ones_own_coin_clears_it();
    training_cpus_coin();
    leaving_screen_says_nothing();
    forgetting_makes_the_next_frame_an_arrival();
    every_portrait_has_a_name();
    free_hand_enters_and_leaves_each_button();
    moving_within_a_button_says_it_once();
    slot_kinds_in_turn();
    ones_own_slot_opening_as_the_hand_reaches_the_portraits_says_only_the_coin();
    slider_grabbed_moved_and_released();
    handicap_slider_says_its_value();
    team_match_teams_button_and_team_button();
    ready_to_fight_appears_and_disappears();
    ready_to_fight_follows_the_local_hands_doing();
    another_player_offline_says_nothing();
    another_player_online_is_queued();
    another_players_choice_that_makes_it_ready_is_queued_with_it();
    choosing_for_a_cpu_says_nothing();
    rules_screen_and_name_entry_open_and_return();
    name_tag_window_opening();
    a_step_names_where_it_goes();
    a_glide_crosses_portraits_in_silence_and_arrives_silently();
    a_glide_ending_elsewhere_names_where_the_hand_is();
    a_failed_glide_says_so();
    a_free_hand_gliding_over_a_button_says_nothing();
    the_coin_jumping_in_during_a_glide_waits_behind_the_step();
    a_step_to_a_coin_says_whose_it_is();
    a_step_to_the_top_bar_with_a_coin_says_it_waits();
    a_step_down_with_ones_own_coin_clears_the_character_first();
    a_step_down_with_a_cpus_coin_puts_it_back_first();
    a_drop_after_a_step_that_did_not_leave_the_portraits_is_said();
    slider_steps_say_the_value_at_the_press_only();
    a_slider_glide_ending_on_another_value_says_it();
    std::cout << "css_speech: all tests passed\n";
    return 0;
}
