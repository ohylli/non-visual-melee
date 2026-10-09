/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage select speech against a fake screen reader bridge, fed snapshots of
 * the screen written by hand (test_sss_screen.hpp). Links sss_speech.cpp,
 * sss_names.cpp and speech.cpp only, so this file supplies the pc_log_line
 * that main.c normally provides. */
#include "speech.hpp"
#include "sss_names.hpp"
#include "sss_speech.hpp"
#include "test_sss_screen.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

using namespace sss_test;

constexpr const char* kOpening = "Stage select. D-pad to choose, Start picks a random stage.";

/* Speech over a fake bridge, and stage select speech over that; s_lines only
 * ever holds the lines of the test that is running. */
struct Fixture {
    std::vector<Output> outputs;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(outputs)};
    a11y::SssSpeech sss{speech};

    Fixture() {
        speech.init();
        s_lines.clear();
    }

    /* Arrives on the screen and drops the opening. */
    explicit Fixture(const A11ySssState& arrival) : Fixture() {
        sss.frame(arrival);
        spoken();
        s_lines.clear();
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
};

using Texts = std::vector<std::string>;

/* A stage tile's hover as spoken: its name, then its description. */
std::string described(A11yStage stage) {
    return std::string(*a11y::stage_name(stage)) + ". " +
           std::string(*a11y::stage_description(stage));
}

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

/* The stage cursor enters a tile. */
void hover(Fixture& f, A11ySssState& state, int tile) {
    state.hovered = tile;
    f.sss.frame(state);
}

/* A or Start reached the pick on the screen's last frame, which made it, with
 * the hovered index it left: Random leaves the stage it rolled there. */
void pick(Fixture& f, A11ySssState& state, int hovered_after) {
    state.pick = A11Y_SSS_PICKED;
    state.hovered = hovered_after;
    f.sss.frame(state);
}

void opening_for_every_controller() {
    Fixture f;
    f.sss.frame(screen());
    assert((f.spoken() == Texts{kOpening}));
}

void opening_for_controller_1_alone() {
    /* The Turns rule with player 1's turn: the cursor is controller 1's,
     * which steering reaches. */
    Fixture f;
    A11ySssState state = screen();
    state.owner = 0;
    f.sss.frame(state);
    assert((f.spoken() == Texts{kOpening}));
}

void opening_for_another_player() {
    Fixture f;
    A11ySssState state = screen();
    state.owner = 2;
    f.sss.frame(state);
    assert((f.spoken() ==
            Texts{std::string(kOpening) + " Player 3 chooses. Steering needs controller 1."}));
}

void opening_online_names_no_player() {
    /* Online the cursor is the local player's, on controller 1, whichever
     * player they are. */
    Fixture f;
    A11ySssState state = screen();
    state.online = true;
    state.owner = 1;
    f.sss.frame(state);
    assert((f.spoken() == Texts{kOpening}));
}

void unchanged_snapshot_speaks_nothing() {
    A11ySssState state = screen();
    Fixture f(state);
    f.sss.frame(state);
    f.sss.frame(state);
    assert(f.spoken().empty());
}

void hover_names_each_stage_entered() {
    A11ySssState state = screen();
    Fixture f(state);
    hover(f, state, kBattlefieldTile);
    hover(f, state, kFoursideTile);
    hover(f, state, kOnettTile);
    hover(f, state, kCorneriaTile);
    assert((f.spoken() == Texts{described(A11Y_St_Kind_Battle), described(A11Y_St_Kind_Fourside),
                              described(A11Y_St_Kind_Onett), described(A11Y_St_Kind_Corneria)}));
}

void hover_says_the_name_and_the_description_as_one() {
    A11ySssState state = screen();
    Fixture f(state);
    hover(f, state, kCorneriaTile);
    assert((f.spoken() ==
            Texts{"Corneria. A fight on the hull of a big ship flying over a city. Arwings fly "
                  "past and fire, and so does the ship's cannon."}));
}

void hover_names_the_random_tile() {
    A11ySssState state = screen();
    Fixture f(state);
    hover(f, state, A11Y_SSS_RANDOM_TILE);
    assert((f.spoken() == Texts{"Random"}));
}

void hover_on_a_locked_tile_hides_its_stage() {
    A11ySssState state = screen_with_locked_stages();
    Fixture f(state);
    hover(f, state, kOnettTile);
    hover(f, state, kFoursideTile);
    assert((f.spoken() == Texts{described(A11Y_St_Kind_Onett), "Locked stage"}));
}

void leaving_a_tile_into_empty_space_says_nothing() {
    /* The game keeps the last tile hovered while the cursor crosses empty
     * space, so the snapshot does not change; entering the same tile again
     * says nothing either. */
    A11ySssState state = screen();
    Fixture f(state);
    hover(f, state, kCorneriaTile);
    f.sss.frame(state);
    f.sss.frame(state);
    assert((f.spoken() == Texts{described(A11Y_St_Kind_Corneria)}));
}

void random_says_the_stage_it_rolled() {
    A11ySssState state = screen();
    Fixture f(state);
    hover(f, state, A11Y_SSS_RANDOM_TILE);
    f.spoken();
    state.pressed_a = true;
    pick(f, state, kCorneriaTile);
    assert((f.spoken() == Texts{"Random: Corneria."}));
    /* The preview shows, then the match loads: nothing more. */
    state.pressed_a = false;
    f.sss.frame(state);
    state.pick = A11Y_SSS_LEAVING;
    f.sss.frame(state);
    assert(f.spoken().empty());
}

void start_before_any_tile_rolls_too() {
    A11ySssState state = screen();
    Fixture f(state);
    state.pressed_start = true;
    pick(f, state, kFountainTile);
    assert((f.spoken() == Texts{"Random: Fountain of Dreams."}));
}

void a_before_any_tile_says_nothing() {
    A11ySssState state = screen();
    Fixture f(state);
    state.pressed_a = true;
    f.sss.frame(state);
    assert(f.spoken().empty());
}

void a_named_pick_says_nothing() {
    A11ySssState state = screen();
    Fixture f(state);
    hover(f, state, kPeachsCastleTile);
    f.spoken();
    state.pressed_a = true;
    pick(f, state, kPeachsCastleTile);
    state.pressed_a = false;
    state.pick = A11Y_SSS_LEAVING;
    f.sss.frame(state);
    assert(f.spoken().empty());
}

void a_pick_on_a_locked_tile_is_refused() {
    A11ySssState state = screen_with_locked_stages();
    Fixture f(state);
    hover(f, state, kBrinstarDepthsTile);
    f.spoken();
    state.pressed_a = true;
    f.sss.frame(state);
    state.pressed_a = false;
    f.sss.frame(state);
    state.pressed_start = true;
    f.sss.frame(state);
    assert((f.spoken() == Texts{"Locked.", "Locked."}));
}

void a_refusal_names_the_tile_entered_after_it() {
    /* The pick acts on the tile hovered before its frame's movement; the
     * cursor may enter another in the same frame. */
    A11ySssState state = screen_with_locked_stages();
    Fixture f(state);
    hover(f, state, kBrinstarDepthsTile);
    f.spoken();
    state.pressed_a = true;
    hover(f, state, kBrinstarTile);
    assert((f.spoken() == Texts{"Locked. " + described(A11Y_St_Kind_Zebes)}));
}

void a_on_an_unlocked_tile_that_did_not_pick_says_nothing() {
    /* Not a refusal: only a locked tile refuses. */
    A11ySssState state = screen();
    Fixture f(state);
    hover(f, state, kBrinstarTile);
    f.spoken();
    state.pressed_a = true;
    f.sss.frame(state);
    assert(f.spoken().empty());
}

/* Stage select in a netplay session, on no tile yet. */
A11ySssState online_screen() {
    A11ySssState state = screen();
    state.online = true;
    state.owner = 1;
    return state;
}

void an_online_pick_says_to_wait() {
    A11ySssState state = online_screen();
    Fixture f(state);
    hover(f, state, kCorneriaTile);
    f.spoken();
    state.pressed_a = true;
    pick(f, state, kCorneriaTile);
    assert((f.spoken() == Texts{"Waiting for your opponent."}));
    /* The preview shows, then the screen waits for the other pick. */
    state.pressed_a = false;
    f.sss.frame(state);
    state.pick = A11Y_SSS_LEAVING;
    f.sss.frame(state);
    assert(f.spoken().empty());
}

void an_online_random_pick_waits_for_its_roll() {
    /* Online Random is rolled only once both players have picked; until
     * then the hovered index keeps the random tile, or none. */
    A11ySssState state = online_screen();
    Fixture f(state);
    hover(f, state, A11Y_SSS_RANDOM_TILE);
    f.spoken();
    state.pressed_a = true;
    pick(f, state, A11Y_SSS_RANDOM_TILE);
    assert((f.spoken() == Texts{"Waiting for your opponent."}));

    A11ySssState start = online_screen();
    Fixture g(start);
    start.pressed_start = true;
    pick(g, start, A11Y_SSS_NO_TILE);
    assert((g.spoken() == Texts{"Waiting for your opponent."}));
}

void the_opponents_pick_is_not_said_before_the_result() {
    A11ySssState state = online_screen();
    Fixture f(state);
    hover(f, state, kCorneriaTile);
    state.pressed_a = true;
    pick(f, state, kCorneriaTile);
    f.spoken();
    state.pressed_a = false;
    state.local_pick = kCorneriaTile;
    state.pick = A11Y_SSS_LEAVING;
    f.sss.frame(state);
    state.remote_pick = kOnettTile;
    f.sss.frame(state);
    assert(f.spoken().empty());
}

/* Online stage select once the local player has picked and waits, then the
 * coin flip's result: the stage's tile and the two picks. */
Texts result(int stage_tile, int local_pick, int remote_pick) {
    A11ySssState state = online_screen();
    Fixture f(state);
    state.pick = A11Y_SSS_LEAVING;
    f.sss.frame(state);
    f.spoken();
    f.sss.resolved(state, stage_tile, local_pick, remote_pick);
    return f.spoken();
}

void the_result_says_whose_pick_won() {
    assert((result(kCorneriaTile, kCorneriaTile, kOnettTile) == Texts{"Corneria, your pick."}));
    assert((result(kOnettTile, kCorneriaTile, kOnettTile) == Texts{"Onett, opponent's pick."}));
}

void the_result_of_one_stage_picked_twice_names_it_alone() {
    assert((result(kCorneriaTile, kCorneriaTile, kCorneriaTile) == Texts{"Corneria."}));
}

void a_random_pick_that_won_says_its_roll_and_whose_it_was() {
    /* Random is the random tile or Start before any tile. */
    assert((result(kFountainTile, A11Y_SSS_RANDOM_TILE, kOnettTile) ==
            Texts{"Random: Fountain of Dreams, your pick."}));
    assert((result(kFountainTile, kOnettTile, A11Y_SSS_NO_TILE) ==
            Texts{"Random: Fountain of Dreams, opponent's pick."}));
    /* The named pick that won beside a Random one. */
    assert((result(kOnettTile, A11Y_SSS_NO_TILE, kOnettTile) == Texts{"Onett, opponent's pick."}));
    assert((result(kOnettTile, kOnettTile, A11Y_SSS_RANDOM_TILE) == Texts{"Onett, your pick."}));
}

void two_random_picks_say_the_roll_alone() {
    assert((result(kBattlefieldTile, A11Y_SSS_RANDOM_TILE, A11Y_SSS_NO_TILE) ==
            Texts{"Random: Battlefield."}));
}

void the_result_is_said_once() {
    A11ySssState state = online_screen();
    Fixture f(state);
    state.pick = A11Y_SSS_LEAVING;
    f.sss.frame(state);
    f.spoken();
    f.sss.resolved(state, kCorneriaTile, kCorneriaTile, kOnettTile);
    f.sss.resolved(state, kCorneriaTile, kCorneriaTile, kOnettTile);
    assert((f.spoken() == Texts{"Corneria, your pick."}));
    /* The next visit says its own. */
    f.sss.forget();
    f.sss.frame(state);
    f.spoken();
    f.sss.resolved(state, kOnettTile, kCorneriaTile, kOnettTile);
    assert((f.spoken() == Texts{"Onett, opponent's pick."}));
}

void offline_the_resolution_says_nothing() {
    /* The game reaches the same line offline, where the pick was said. */
    A11ySssState state = screen();
    Fixture f(state);
    state.pick = A11Y_SSS_LEAVING;
    f.sss.frame(state);
    f.sss.resolved(state, kCorneriaTile, -1, -1);
    assert(f.spoken().empty());
}

/* The stage-select rule Random or Ordered: the scene leaves on its first
 * frame with the stage already chosen. */
A11ySssState skipped_scene(A11yStage stage) {
    A11ySssState state{};
    state.skipped = true;
    state.skipped_stage = stage;
    return state;
}

void skipped_scene_names_the_stage_instead_of_the_opening() {
    Fixture f;
    f.sss.frame(skipped_scene(A11Y_St_Kind_Corneria));
    assert((f.spoken() == Texts{"Stage: Corneria."}));
}

void skipped_scene_names_the_stage_once() {
    Fixture f;
    A11ySssState state = skipped_scene(A11Y_St_Kind_OldPupupu);
    f.sss.frame(state);
    f.sss.frame(state);
    assert((f.spoken() == Texts{"Stage: Dream Land N64."}));
}

void skipped_scene_with_a_stage_missing_from_the_table() {
    Fixture f;
    f.sss.frame(skipped_scene(static_cast<A11yStage>(0x15)));
    assert((f.spoken() == Texts{"Stage: Unknown stage 21."}));
    assert(count_logged("[a11y] stage 21: not in the stage names table") == 1);
}

void stage_select_after_a_skipped_scene_says_its_opening() {
    /* The rule changed back to On between two matches. */
    Fixture f;
    f.sss.frame(skipped_scene(A11Y_St_Kind_Corneria));
    f.spoken();
    f.sss.forget();
    f.sss.frame(screen());
    assert((f.spoken() == Texts{kOpening}));
}

void forgetting_makes_the_next_frame_an_arrival() {
    A11ySssState state = screen();
    Fixture f(state);
    hover(f, state, kCorneriaTile);
    f.spoken();
    f.sss.forget();
    f.sss.frame(screen());
    assert((f.spoken() == Texts{kOpening}));
}

void missing_name_is_spoken_by_number_and_logged_once() {
    A11ySssState state = screen();
    Fixture f(state);
    /* Akaneia, a stage the grid does not have. */
    state.tiles[kCorneriaTile].stage = static_cast<A11yStage>(0x15);
    hover(f, state, kCorneriaTile);
    hover(f, state, kOnettTile);
    hover(f, state, kCorneriaTile);
    assert((f.spoken() ==
            Texts{"Unknown stage 21", described(A11Y_St_Kind_Onett), "Unknown stage 21"}));
    assert(count_logged("[a11y] stage 21: not in the stage names table") == 1);
}

void every_tile_has_a_name() {
    for (const TileRow& row : kTiles) {
        assert(row.stage == A11Y_NO_STAGE || a11y::stage_name(row.stage).has_value());
    }
    assert(a11y::stage_name(A11Y_St_Kind_Shrine) == "Hyrule Temple");
    assert(a11y::stage_name(A11Y_St_Kind_OldPupupu) == "Dream Land N64");
    assert(!a11y::stage_name(A11Y_NO_STAGE).has_value());
}

void every_tile_has_a_description_of_one_or_two_sentences() {
    for (const TileRow& row : kTiles) {
        if (row.stage == A11Y_NO_STAGE) {
            continue;
        }
        std::optional<std::string_view> text = a11y::stage_description(row.stage);
        assert(text.has_value() && !text->empty());
        assert(text->back() == '.');
        std::size_t sentences = 0;
        for (std::size_t i = 0; i < text->size(); i++) {
            sentences += (*text)[i] == '.' && (i + 1 == text->size() || (*text)[i + 1] == ' ');
        }
        assert(sentences == 1 || sentences == 2);
    }
    assert(!a11y::stage_description(A11Y_NO_STAGE).has_value());
}

void glide_words_name_the_stage_alone() {
    /* "Could not reach Corneria" and "Random: Corneria." say no
     * description. */
    A11ySssState state = screen();
    Fixture f(state);
    assert(f.sss.tile_words(state, kCorneriaTile) == "Corneria");
    assert(f.sss.hover_words(state, kCorneriaTile) == described(A11Y_St_Kind_Corneria));
    assert(f.sss.hover_words(state, A11Y_SSS_RANDOM_TILE) == "Random");
}

using Glide = a11y::GlideState<int>;
using a11y::GlidePhase;

void a_step_names_its_destination_at_the_press() {
    A11ySssState state = screen();
    Fixture f(state);
    f.sss.step(state, kCorneriaTile);
    f.sss.step(state, A11Y_SSS_RANDOM_TILE);
    assert((f.spoken() == Texts{described(A11Y_St_Kind_Corneria), "Random"}));
    /* At an edge the step names the same tile again. */
    f.sss.step(state, A11Y_SSS_RANDOM_TILE);
    assert((f.spoken() == Texts{"Random"}));
    f.sss.step(screen_with_locked_stages(), kFoursideTile);
    assert((f.spoken() == Texts{"Locked stage"}));
}

void a_glide_crosses_tiles_and_arrives_in_silence() {
    A11ySssState state = screen();
    Fixture f(state);
    f.sss.step(state, kFountainTile);
    f.spoken();
    Glide glide{GlidePhase::gliding, kFountainTile};
    state.hovered = kBattlefieldTile;
    f.sss.frame(state, glide);
    state.hovered = kFoursideTile;
    f.sss.frame(state, glide);
    state.hovered = kFountainTile;
    f.sss.frame(state, glide);
    f.sss.frame(state, Glide{GlidePhase::ended, kFountainTile});
    f.sss.frame(state);
    assert(f.spoken().empty());
    /* The stick takes over from there: hovers are said again. */
    hover(f, state, kCorneriaTile);
    assert((f.spoken() == Texts{described(A11Y_St_Kind_Corneria)}));
}

void a_glide_ending_elsewhere_names_where_the_cursor_is() {
    /* The player's stick took over on Onett. */
    A11ySssState state = screen();
    Fixture f(state);
    f.sss.step(state, kFountainTile);
    f.spoken();
    state.hovered = kOnettTile;
    f.sss.frame(state, Glide{GlidePhase::gliding, kFountainTile});
    assert(f.spoken().empty());
    f.sss.frame(state, Glide{GlidePhase::ended, kFountainTile});
    assert((f.spoken() == Texts{described(A11Y_St_Kind_Onett)}));
}

void a_failed_glide_says_so_and_where_the_cursor_is() {
    A11ySssState state = screen();
    Fixture f(state);
    f.sss.step(state, kCorneriaTile);
    f.spoken();
    state.hovered = kFountainTile;
    f.sss.frame(state, Glide{GlidePhase::gliding, kCorneriaTile});
    f.sss.frame(state, Glide{GlidePhase::failed, kCorneriaTile});
    assert((f.spoken() == Texts{"Could not reach Corneria. " + described(A11Y_St_Kind_Izumi)}));
    /* Failed on the tile itself: only that it failed. */
    state.hovered = kCorneriaTile;
    f.sss.frame(state, Glide{GlidePhase::gliding, kCorneriaTile});
    f.sss.frame(state, Glide{GlidePhase::failed, kCorneriaTile});
    assert((f.spoken() == Texts{"Could not reach Corneria"}));
}

void a_pick_during_a_glide_names_the_stage_picked() {
    /* The step named Fountain of Dreams; A while the cursor crossed Onett
     * picks Onett, and the glide stops with the pick. */
    A11ySssState state = screen();
    Fixture f(state);
    f.sss.step(state, kFountainTile);
    f.spoken();
    state.hovered = kOnettTile;
    f.sss.frame(state, Glide{GlidePhase::gliding, kFountainTile});
    state.pressed_a = true;
    state.pick = A11Y_SSS_PICKED;
    f.sss.frame(state, Glide{GlidePhase::ended, kFountainTile});
    assert((f.spoken() == Texts{"Picked Onett."}));
    state.pressed_a = false;
    state.pick = A11Y_SSS_LEAVING;
    f.sss.frame(state);
    assert(f.spoken().empty());
}

void a_pick_where_the_step_went_says_nothing() {
    A11ySssState state = screen();
    Fixture f(state);
    f.sss.step(state, kFountainTile);
    f.spoken();
    state.hovered = kFountainTile;
    f.sss.frame(state, Glide{GlidePhase::gliding, kFountainTile});
    f.sss.frame(state, Glide{GlidePhase::ended, kFountainTile});
    state.pressed_a = true;
    state.pick = A11Y_SSS_PICKED;
    f.sss.frame(state);
    assert(f.spoken().empty());
}

void a_pick_during_a_glide_still_rolls_random() {
    /* A on the random tile crossed on the way: its roll, no "Picked". */
    A11ySssState state = screen();
    Fixture f(state);
    f.sss.step(state, kBrinstarTile);
    f.spoken();
    state.hovered = A11Y_SSS_RANDOM_TILE;
    f.sss.frame(state, Glide{GlidePhase::gliding, kBrinstarTile});
    state.pressed_a = true;
    state.pick = A11Y_SSS_PICKED;
    state.hovered = kCorneriaTile;
    f.sss.frame(state, Glide{GlidePhase::ended, kBrinstarTile});
    assert((f.spoken() == Texts{"Random: Corneria."}));
}

void an_online_pick_during_a_glide_names_the_stage_picked() {
    A11ySssState state = online_screen();
    Fixture f(state);
    f.sss.step(state, kFountainTile);
    f.spoken();
    state.hovered = kOnettTile;
    f.sss.frame(state, Glide{GlidePhase::gliding, kFountainTile});
    state.pressed_a = true;
    state.pick = A11Y_SSS_PICKED;
    f.sss.frame(state, Glide{GlidePhase::ended, kFountainTile});
    assert((f.spoken() == Texts{"Picked Onett. Waiting for your opponent."}));
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
    opening_for_every_controller();
    opening_for_controller_1_alone();
    opening_for_another_player();
    opening_online_names_no_player();
    unchanged_snapshot_speaks_nothing();
    hover_names_each_stage_entered();
    hover_says_the_name_and_the_description_as_one();
    hover_names_the_random_tile();
    hover_on_a_locked_tile_hides_its_stage();
    leaving_a_tile_into_empty_space_says_nothing();
    random_says_the_stage_it_rolled();
    start_before_any_tile_rolls_too();
    a_before_any_tile_says_nothing();
    a_named_pick_says_nothing();
    a_pick_on_a_locked_tile_is_refused();
    a_refusal_names_the_tile_entered_after_it();
    a_on_an_unlocked_tile_that_did_not_pick_says_nothing();
    an_online_pick_says_to_wait();
    an_online_random_pick_waits_for_its_roll();
    the_opponents_pick_is_not_said_before_the_result();
    the_result_says_whose_pick_won();
    the_result_of_one_stage_picked_twice_names_it_alone();
    a_random_pick_that_won_says_its_roll_and_whose_it_was();
    two_random_picks_say_the_roll_alone();
    the_result_is_said_once();
    offline_the_resolution_says_nothing();
    skipped_scene_names_the_stage_instead_of_the_opening();
    skipped_scene_names_the_stage_once();
    skipped_scene_with_a_stage_missing_from_the_table();
    stage_select_after_a_skipped_scene_says_its_opening();
    forgetting_makes_the_next_frame_an_arrival();
    missing_name_is_spoken_by_number_and_logged_once();
    every_tile_has_a_name();
    every_tile_has_a_description_of_one_or_two_sentences();
    glide_words_name_the_stage_alone();
    a_step_names_its_destination_at_the_press();
    a_glide_crosses_tiles_and_arrives_in_silence();
    a_glide_ending_elsewhere_names_where_the_cursor_is();
    a_failed_glide_says_so_and_where_the_cursor_is();
    a_pick_during_a_glide_names_the_stage_picked();
    a_pick_where_the_step_went_says_nothing();
    a_pick_during_a_glide_still_rolls_random();
    an_online_pick_during_a_glide_names_the_stage_picked();
    std::cout << "sss_speech: all tests passed\n";
    return 0;
}
