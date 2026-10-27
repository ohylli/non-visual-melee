/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Results speech against hand-written snapshots of the results scene, over a
 * fake screen reader bridge. Links results_speech.cpp, results_names.cpp,
 * css_names.cpp, game_text.cpp and speech.cpp only, so this file supplies the
 * pc_log_line that main.c normally provides. */
#include "results_speech.hpp"
#include "speech.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
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

using Bytes = std::vector<std::uint8_t>;

/* NTSC-U game text for ASCII letters and spaces. */
Bytes game_text(const char* text) {
    Bytes out;
    auto glyph = [&out](unsigned code) {
        out.push_back(static_cast<std::uint8_t>(code >> 8));
        out.push_back(static_cast<std::uint8_t>(code));
    };
    for (; *text != '\0'; text++) {
        char c = *text;
        if (c == ' ') {
            out.push_back(0x1A);
        } else if (c >= 'A' && c <= 'Z') {
            glyph(0x200A + (c - 'A'));
        } else if (c >= 'a' && c <= 'z') {
            glyph(0x2024 + (c - 'a'));
        }
    }
    out.push_back(0x00);
    return out;
}

/* The match types' titles, as SdRst holds them. */
const Bytes kTimeTitle = game_text("Time Battle");
const Bytes kStockTitle = game_text("Stock Battle");
const Bytes kCoinTitle = game_text("Coin Battle");
const Bytes kBonusTitle = game_text("Bonus Battle");

const Bytes& title_of(A11yResultsMatchKind kind) {
    switch (kind) {
    case A11Y_RESULTS_TIME:
        return kTimeTitle;
    case A11Y_RESULTS_STOCK:
        return kStockTitle;
    case A11Y_RESULTS_COIN:
        return kCoinTitle;
    case A11Y_RESULTS_BONUS:
        break;
    }
    return kBonusTitle;
}

A11yResultsPlayer human(A11yCharacter character, int place, int rank) {
    A11yResultsPlayer player{};
    player.slot = A11Y_RESULTS_HUMAN;
    player.character = character;
    player.place = place;
    player.rank = rank;
    return player;
}

A11yResultsPlayer cpu(A11yCharacter character, int place, int rank) {
    A11yResultsPlayer player = human(character, place, rank);
    player.slot = A11Y_RESULTS_CPU;
    return player;
}

A11yResultsPlayer& stats(A11yResultsPlayer& player, int kos, int falls, int sds) {
    player.kos = kos;
    player.falls = falls;
    player.sds = sds;
    return player;
}

/* The spec's example: a four-player Stock match, you on P1. DK on P4 won;
 * Link on P1 came 2nd, Fox on P3 3rd and Mario on P2 4th. */
A11yResults stock_example() {
    A11yResults results{};
    results.valid = true;
    results.state = A11Y_RESULTS_ANNOUNCING;
    results.match_kind = A11Y_RESULTS_STOCK;
    results.winner = 3;
    results.local_port = 0;
    results.players[0] = human(A11Y_CKind_Link, 1, 1);
    stats(results.players[0], 1, 1, 0);
    results.players[0].knocked_out = true;
    results.players[0].ko_seconds = 106;
    results.players[1] = cpu(A11Y_CKind_Mario, 3, 3);
    stats(results.players[1], 0, 1, 0);
    results.players[1].knocked_out = true;
    results.players[1].ko_seconds = 76;
    results.players[2] = cpu(A11Y_CKind_Fox, 2, 2);
    stats(results.players[2], 0, 1, 0);
    results.players[2].knocked_out = true;
    results.players[2].ko_seconds = 87;
    results.players[3] = cpu(A11Y_CKind_Donkey, 0, 0);
    stats(results.players[3], 2, 0, 0);
    return results;
}

/* Two players: you as Link on P1, a CPU Mario on P2, the CPU ahead. */
A11yResults two_players(A11yResultsMatchKind kind) {
    A11yResults results{};
    results.valid = true;
    results.match_kind = kind;
    results.winner = 1;
    results.local_port = 0;
    results.players[0] = human(A11Y_CKind_Link, 1, 1);
    results.players[1] = cpu(A11Y_CKind_Mario, 0, 0);
    return results;
}

struct Fixture {
    Outputs outputs;
    a11y::Speech speech{a11y::Config{true, true}, std::make_unique<FakeBridge>(outputs)};
    a11y::ResultsSpeech reader{speech, a11y::GameTextSource{}};

    Fixture() {
        speech.init();
        s_lines.clear();
        reader.visit_started();
    }

    /* One tick of the scene in the state given, its title the match
     * type's; returns what it said, clearing it. */
    Outputs tick(A11yResults results, A11yResultsState state) {
        results.state = state;
        if (results.match_type_text == nullptr) {
            results.match_type_text = title_of(results.match_kind).data();
        }
        reader.tick(results);
        Outputs said = std::move(outputs);
        outputs.clear();
        return said;
    }

    /* A visit from entry to the summary table, the ticks before it silent;
     * returns the opening. */
    std::string visit(const A11yResults& results) {
        reader.visit_started();
        assert(tick(results, A11Y_RESULTS_ANNOUNCING).empty());
        assert(tick(results, A11Y_RESULTS_SLIDING).empty());
        Outputs said = tick(results, A11Y_RESULTS_SUMMARY);
        assert(said.size() == 1);
        assert(said[0].interrupt);
        return said[0].text;
    }
};

constexpr const char* kKeys = " Any button shows the stats.";

std::size_t count_logged(const std::string& line) {
    return static_cast<std::size_t>(std::count(s_lines.begin(), s_lines.end(), line));
}

void opening_waits_for_the_summary_table() {
    Fixture f;
    A11yResults results = stock_example();
    for (int i = 0; i < 200; i++) {
        assert(f.tick(results, A11Y_RESULTS_ANNOUNCING).empty());
    }
    assert(f.tick(results, A11Y_RESULTS_SLIDING).empty());
    assert(f.tick(results, A11Y_RESULTS_SUMMARY).size() == 1);
    /* Once a visit, whatever comes after. */
    assert(f.tick(results, A11Y_RESULTS_SUMMARY).empty());
    assert(f.tick(results, A11Y_RESULTS_STATS).empty());
    assert(f.tick(results, A11Y_RESULTS_LEAVING).empty());
}

void stock_opening_in_place_order() {
    Fixture f;
    assert(f.visit(stock_example()) ==
           std::string("Stock Battle. Winner: DK. 1st DK, P4, 2 KOs, 0 falls, 0 SDs, not KO'd. "
                       "2nd Link, P1, you, 1 KO, 1 fall, 0 SDs, KO'd at 1:46. 3rd Fox, P3, 0 "
                       "KOs, 1 fall, 0 SDs, KO'd at 1:27. 4th Mario, P2, 0 KOs, 1 fall, 0 SDs, "
                       "KO'd at 1:16.") +
               kKeys + " Start when ready.");
}

void keys_on_the_first_results_of_the_run_only() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_STOCK);
    std::string first = f.visit(results);
    assert(first.ends_with(std::string(kKeys) + " Start when ready."));
    std::string second = f.visit(results);
    assert(second ==
           "Stock Battle. Winner: Mario. 1st Mario, P2, 0 KOs, 0 falls, 0 SDs, not KO'd. 2nd "
           "Link, P1, you, 0 KOs, 0 falls, 0 SDs, not KO'd. Start when ready.");
    /* Nor has a third. */
    assert(f.visit(results) == second);
}

void opening_said_when_the_summary_is_passed_unseen() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_STOCK);
    assert(f.tick(results, A11Y_RESULTS_SLIDING).empty());
    assert(f.tick(results, A11Y_RESULTS_STATS).size() == 1);
}

void time_totals_with_their_sign() {
    Fixture f;
    A11yResults results = stock_example();
    results.match_kind = A11Y_RESULTS_TIME;
    results.players[3].score = 2;
    results.players[0].score = 0;
    results.players[2].score = -1;
    results.players[1].score = -1;
    std::string said = f.visit(results);
    assert(said.starts_with("Time Battle. Winner: DK. 1st DK, P4, 2 KOs, 0 falls, 0 SDs, total "
                            "+2. 2nd Link, P1, you, 1 KO, 1 fall, 0 SDs, total 0. 3rd Fox, P3, 0 "
                            "KOs, 1 fall, 0 SDs, total -1. 4th Mario, P2, 0 KOs, 1 fall, 0 SDs, "
                            "total -1."));
}

void coin_counts() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_COIN);
    results.players[1].coins = 120;
    results.players[0].coins = 1;
    std::string said = f.visit(results);
    assert(said.starts_with("Coin Battle. Winner: Mario. 1st Mario, P2, 0 KOs, 0 falls, 0 SDs, "
                            "120 coins. 2nd Link, P1, you, 0 KOs, 0 falls, 0 SDs, 1 coin."));
}

void bonus_mode_says_points_alone() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_BONUS);
    stats(results.players[0], 3, 2, 1);
    results.players[1].score = 19400;
    results.players[0].score = -1000;
    std::string said = f.visit(results);
    assert(said.starts_with("Bonus Battle. Winner: Mario. 1st Mario, P2, 19400 points. 2nd Link, "
                            "P1, you, -1000 points."));
}

void no_contest_in_port_order_without_places() {
    Fixture f;
    A11yResults results = stock_example();
    results.cancelled = true;
    for (A11yResultsPlayer& player : results.players) {
        player.place = -1;
    }
    std::string said = f.visit(results);
    assert(said.starts_with("Stock Battle. No contest. Link, P1, you, 1 KO, 1 fall, 0 SDs, KO'd "
                            "at 1:46. Mario, P2, 0 KOs, 1 fall, 0 SDs, KO'd at 1:16. Fox, P3, 0 "
                            "KOs, 1 fall, 0 SDs, KO'd at 1:27. DK, P4, 2 KOs, 0 falls, 0 SDs, not "
                            "KO'd."));
    assert(said.find("Winner") == std::string::npos);
}

void bonus_no_contest_leaves_the_points_out() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_BONUS);
    results.cancelled = true;
    results.players[0].place = -1;
    results.players[1].place = -1;
    std::string said = f.visit(results);
    assert(said.starts_with("Bonus Battle. No contest. Link, P1, you. Mario, P2. "));
}

void team_match_names_the_team() {
    Fixture f;
    A11yResults results = stock_example();
    results.teams = true;
    results.winning_team = 1;
    /* DK (P4) and Fox (P3) on blue, 1st together; Link (P1) and Mario (P2)
     * on red, 2nd together. */
    results.players[3].team = 1;
    results.players[2].team = 1;
    results.players[2].place = 0;
    results.players[0].team = 0;
    results.players[1].team = 0;
    results.players[1].place = 1;
    results.players[3].knocked_out = false;
    results.players[2].knocked_out = false;
    std::string said = f.visit(results);
    assert(said.starts_with("Stock Battle. Winner: Blue team. 1st DK, P4, Blue team, 2 KOs, 0 "
                            "falls, 0 SDs, not KO'd. 1st Fox, P3, Blue team, 0 KOs, 1 fall, 0 "
                            "SDs, not KO'd. 2nd Link, P1, you, Red team, 1 KO, 1 fall, 0 SDs, "
                            "KO'd at 1:46. 2nd Mario, P2, Red team, 0 KOs, 1 fall, 0 SDs, KO'd "
                            "at 1:16."));
}

void tied_players_share_a_place() {
    Fixture f;
    A11yResults results = stock_example();
    /* Fox and Mario out on the same frame: both 3rd, Fox ahead by the
     * game's own order, said first though on the later port. */
    results.players[1].place = 2;
    results.players[1].rank = 3;
    results.players[2].rank = 2;
    results.players[1].ko_seconds = 87;
    std::string said = f.visit(results);
    assert(said.find("3rd Fox, P3, 0 KOs, 1 fall, 0 SDs, KO'd at 1:27. 3rd Mario, P2, 0 KOs, 1 "
                     "fall, 0 SDs, KO'd at 1:27.") != std::string::npos);
}

void the_bannered_winner_leads_a_tie_for_first() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_TIME);
    /* Both 1st with no rank between them; the banner names P2. */
    results.players[0].place = 0;
    results.players[0].rank = 0;
    std::string said = f.visit(results);
    assert(said.starts_with("Time Battle. Winner: Mario. 1st Mario, P2, 0 KOs, 0 falls, 0 SDs, "
                            "total 0. 1st Link, P1, you, "));
}

void nobody_is_you_on_a_cpu_or_empty_port() {
    Fixture f;
    A11yResults results = stock_example();
    results.players[0].slot = A11Y_RESULTS_CPU;
    std::string said = f.visit(results);
    assert(said.find("you") == std::string::npos);
    assert(said.find("2nd Link, P1, 1 KO") != std::string::npos);
    assert(a11y::results_you(results) == -1);

    results.local_port = 0;
    results.players[0] = A11yResultsPlayer{};
    said = f.visit(results);
    assert(said.find("you") == std::string::npos);
    assert(said.find("Link") == std::string::npos);
    assert(a11y::results_you(results) == -1);
}

void online_you_are_the_local_player() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_STOCK);
    results.players[1].slot = A11Y_RESULTS_HUMAN;
    results.local_port = 1;
    std::string said = f.visit(results);
    assert(said.starts_with("Stock Battle. Winner: Mario. 1st Mario, P2, you, 0 KOs, "));
    assert(said.find("2nd Link, P1, 0 KOs") != std::string::npos);
}

void empty_ports_are_left_out() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_STOCK);
    std::string said = f.visit(results);
    assert(said.find("P3") == std::string::npos);
    assert(said.find("P4") == std::string::npos);
    assert((a11y::results_order(results) == std::vector<int>{1, 0}));
}

void unknown_match_type_is_left_out_and_logged_once() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_STOCK);
    results.state = A11Y_RESULTS_SUMMARY;
    /* No table loaded: the title is left out, and logged once over two
     * visits. */
    results.match_type_text = nullptr;
    for (int i = 0; i < 2; i++) {
        f.reader.visit_started();
        f.reader.tick(results);
        assert(f.outputs.size() == 1);
        assert(f.outputs[0].text.starts_with("Winner: Mario. 1st Mario, P2, "));
        f.outputs.clear();
    }
    assert(count_logged("[a11y] results: match type 1: no game text") == 1);
}

void unknown_character_is_named_by_port() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_STOCK);
    results.players[1].character = A11Y_NO_CHARACTER;
    std::string said = f.visit(results);
    assert(said.starts_with("Stock Battle. Winner: P2. 1st P2, 0 KOs, "));
    assert(count_logged("[a11y] results: port 1: character not in the character names table") == 1);
}

void ko_time_formats() {
    Fixture f;
    A11yResults results = two_players(A11Y_RESULTS_STOCK);
    results.players[0].knocked_out = true;
    results.players[0].ko_seconds = 5;
    assert(f.visit(results).find("KO'd at 0:05") != std::string::npos);
    results.players[0].ko_seconds = 5999;
    assert(f.visit(results).find("KO'd at 99:59") != std::string::npos);
}

void nothing_without_a_snapshot() {
    Fixture f;
    A11yResults results{};
    results.state = A11Y_RESULTS_SUMMARY;
    f.reader.tick(results);
    assert(f.outputs.empty());
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
    opening_waits_for_the_summary_table();
    stock_opening_in_place_order();
    keys_on_the_first_results_of_the_run_only();
    opening_said_when_the_summary_is_passed_unseen();
    time_totals_with_their_sign();
    coin_counts();
    bonus_mode_says_points_alone();
    no_contest_in_port_order_without_places();
    bonus_no_contest_leaves_the_points_out();
    team_match_names_the_team();
    tied_players_share_a_place();
    the_bannered_winner_leads_a_tie_for_first();
    nobody_is_you_on_a_cpu_or_empty_port();
    online_you_are_the_local_player();
    empty_ports_are_left_out();
    unknown_match_type_is_left_out_and_logged_once();
    unknown_character_is_named_by_port();
    ko_time_formats();
    nothing_without_a_snapshot();
    std::cout << "results_speech: all tests passed\n";
    return 0;
}
