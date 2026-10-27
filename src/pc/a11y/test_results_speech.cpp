/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Results speech against hand-written snapshots of the results scene, over a
 * fake screen reader bridge: the opening, the stats panels' pages, the
 * reading cursor and ready. Links results_speech.cpp, results_names.cpp,
 * css_names.cpp, game_text.cpp and speech.cpp only, so this file supplies the
 * pc_log_line that main.c normally provides. */
#include "results_names.hpp"
#include "results_speech.hpp"
#include "speech.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
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
    /* Once a visit, whatever comes after; the stats panels speak for
     * themselves. */
    assert(f.tick(results, A11Y_RESULTS_SUMMARY).empty());
    Outputs stats = f.tick(results, A11Y_RESULTS_STATS);
    assert(stats.size() == 1 && !stats[0].text.starts_with("Stock Battle"));
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
    Outputs said = f.tick(results, A11Y_RESULTS_STATS);
    assert(said.size() == 2);
    assert(said[0].text.starts_with("Stock Battle."));
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

/* The stats panels' game text, as SdRst holds it. */
const Bytes kKosHeading = game_text("KOs");
const Bytes kFallsHeading = game_text("Falls");
const Bytes kSdsHeading = game_text("SDs");
const Bytes kEmpty = game_text("");

/* The Stats page's labels: the label row's string and the value row's. */
const std::vector<std::pair<Bytes, Bytes>> kStatLabels = [] {
    std::vector<std::pair<Bytes, Bytes>> labels;
    for (const char* label :
        {"KOs", "Falls", "SDs", "DamageGiven", "DamageTaken", "DamageRecovered", "PeakDamage",
            "FlightDistance", "GroundTime", "AirTime", "HitPercentage", "GroundAttacks",
            "AirAttacks", "SmashAttacks", "Attempted", "Throws", "Cliffhangers", "ItemsThrown",
            "ItemsGrabbed", "TopSpeed", "FastestPitch", "LongestDrought", "Transformation"})
    {
        labels.emplace_back(game_text(label), kEmpty);
    }
    labels[14].second = game_text("Grabs");
    labels[22].second = game_text("Time");
    return labels;
}();

const Bytes kSwitzerland = game_text("Switzerland");
const Bytes kPummeler = game_text("Pummeler");
const Bytes kThrowDown = game_text("Throw Down");
const Bytes kStatue = game_text("Statue");
const Bytes kLastPlace = game_text("Last Place");
const Bytes kStiffKnees = game_text("Stiff Knees");
const Bytes kSpectator = game_text("Spectator");
const Bytes kAmbler = game_text("Ambler");

void add_bonus(A11yResultsPlayer& player, int kind, const Bytes& name, int points) {
    A11yResultsBonus& bonus = player.bonuses[player.bonus_count++];
    bonus.kind = kind;
    bonus.name = name.data();
    bonus.points = points;
}

/* The spec's example with its stats panels. Link (P1, you) KO'd Mario once
 * and was KO'd by DK, who also KO'd Fox. Link dealt 87% and Mario 41%;
 * Link has five bonuses, Mario three, the others none. kills[a][b] is the
 * times a KO'd b. */
A11yResults panels_example() {
    A11yResults results = stock_example();
    results.page_count = 3;
    results.kos_heading = kKosHeading.data();
    results.falls_heading = kFallsHeading.data();
    results.sds_heading = kSdsHeading.data();
    for (int i = 0; i < A11Y_RESULTS_STAT_COUNT; i++) {
        results.stat_labels[i][0] = kStatLabels[i].first.data();
        results.stat_labels[i][1] = kStatLabels[i].second.data();
    }
    int kills[4][4] = {};
    kills[0][1] = 1;
    kills[3][0] = 1;
    kills[3][2] = 1;
    for (int port = 0; port < 4; port++) {
        A11yResultsPlayer& player = results.players[port];
        for (int other = 0; other < 4; other++) {
            if (other != port) {
                A11yResultsOpponent& opponent = player.opponents[player.opponent_count++];
                opponent.port = other;
                opponent.kos = kills[port][other];
                opponent.falls = kills[other][port];
            }
        }
        player.stats[0] = player.kos;
        player.stats[1] = player.falls;
        player.stats[2] = player.sds;
        player.stats[14] = 3;
    }
    results.players[0].stats[3] = 87;
    results.players[1].stats[3] = 41;
    add_bonus(results.players[0], 40, kSwitzerland, 12000);
    add_bonus(results.players[0], 41, kPummeler, 1000);
    add_bonus(results.players[0], 42, kThrowDown, 1000);
    add_bonus(results.players[0], 43, kStatue, 1000);
    add_bonus(results.players[0], 1, kLastPlace, -1000);
    add_bonus(results.players[1], 50, kStiffKnees, 300);
    add_bonus(results.players[1], 51, kSpectator, 1000);
    add_bonus(results.players[1], 52, kAmbler, 2500);
    return results;
}

/* Every panel on a page. */
A11yResults& on_page(A11yResults& results, A11yResultsPage page) {
    for (A11yResultsPlayer& player : results.players) {
        player.page = page;
    }
    return results;
}

constexpr const char* kStatsKeys =
    " A and B change page. D-pad up and down read the rows, left and right switch player. Z the "
    "player's summary. Start when ready.";

/* A press of the local controller's buttons, read by the next tick. */
enum Press { kNone, kUp, kDown, kLeft, kRight, kZ };

struct PanelFixture : Fixture {
    A11yResults results = panels_example();

    /* Through the opening to the panels open on the page given; returns
     * the panels' first announcement. */
    std::string open(A11yResultsPage page = A11Y_RESULTS_PAGE_KOS) {
        on_page(results, page);
        visit(results);
        Outputs said = tick(results, A11Y_RESULTS_STATS);
        assert(said.size() == 1);
        assert(said[0].interrupt);
        return said[0].text;
    }

    /* One tick with a press; returns the one announcement it made, or ""
     * for none. */
    std::string press(Press button) {
        A11yResults now = results;
        now.pressed_up = button == kUp;
        now.pressed_down = button == kDown;
        now.pressed_left = button == kLeft;
        now.pressed_right = button == kRight;
        now.pressed_z = button == kZ;
        Outputs said = tick(now, A11Y_RESULTS_STATS);
        assert(said.size() <= 1);
        if (said.empty()) {
            return "";
        }
        assert(said[0].interrupt);
        return said[0].text;
    }

    /* Your panel turned to a page; returns what was said. */
    std::string page(A11yResultsPage page) {
        results.players[0].page = page;
        return press(kNone);
    }
};

void stats_open_with_the_page_and_its_first_row() {
    PanelFixture f;
    assert(f.open() == std::string("KOs and falls. KOs.") + kStatsKeys);
    /* Nothing more until a press. */
    assert(f.press(kNone).empty());
}

void the_opening_press_reads_no_row() {
    PanelFixture f;
    f.visit(f.results);
    /* The D-pad press that opened the panels, seen on their first tick. */
    A11yResults opened = f.results;
    opened.pressed_down = true;
    Outputs said = f.tick(opened, A11Y_RESULTS_STATS);
    assert(said.size() == 1);
    assert(said[0].text == std::string("KOs and falls. KOs.") + kStatsKeys);
    /* The cursor is still on the first row. */
    assert(f.press(kDown) == "P2 Mario, 1");
    /* Before the panels, a press is the game's alone. */
    PanelFixture g;
    g.visit(g.results);
    A11yResults summary = g.results;
    summary.pressed_z = true;
    assert(g.tick(summary, A11Y_RESULTS_SUMMARY).empty());
}

void stats_opened_by_another_player_and_keys_once_a_run() {
    PanelFixture f;
    /* Nothing of yours pressed: another player's press opened them. */
    f.visit(f.results);
    Outputs said = f.tick(f.results, A11Y_RESULTS_STATS);
    assert(said.size() == 1 && said[0].text == std::string("KOs and falls. KOs.") + kStatsKeys);
    /* The next results of the run: no keys. */
    assert(f.open() == "KOs and falls. KOs.");
}

void stats_opening_queues_behind_an_opening_said_on_the_same_tick() {
    PanelFixture f;
    f.reader.visit_started();
    Outputs said = f.tick(f.results, A11Y_RESULTS_STATS);
    assert(said.size() == 2);
    assert(said[0].text.starts_with("Stock Battle.") && said[0].interrupt);
    assert(said[1].text == std::string("KOs and falls. KOs.") + kStatsKeys && !said[1].interrupt);
}

void page_changes_and_their_silent_ends() {
    PanelFixture f;
    f.open();
    assert(f.page(A11Y_RESULTS_PAGE_STATS) == "Stats. KOs, 1.");
    assert(f.page(A11Y_RESULTS_PAGE_BONUSES) == "Bonuses. Switzerland, 12000.");
    /* A on the last page: the page stays, nothing is said. */
    assert(f.press(kNone).empty());
    assert(f.page(A11Y_RESULTS_PAGE_STATS) == "Stats. KOs, 1.");
    assert(f.page(A11Y_RESULTS_PAGE_KOS) == "KOs and falls. KOs.");
    /* B on the first page. */
    assert(f.press(kNone).empty());
    /* Another player's paging is theirs. */
    f.results.players[1].page = A11Y_RESULTS_PAGE_BONUSES;
    f.results.players[2].page = A11Y_RESULTS_PAGE_STATS;
    assert(f.press(kNone).empty());
}

void a_page_change_goes_to_its_first_row_and_keeps_the_player() {
    PanelFixture f;
    f.open(A11Y_RESULTS_PAGE_STATS);
    f.press(kDown);
    f.press(kDown);
    assert(f.press(kRight) == "Mario, P2, 0");
    assert(f.page(A11Y_RESULTS_PAGE_BONUSES) == "Bonuses. Stiff Knees, 300.");
    assert(f.press(kDown) == "Spectator, 1000");
}

void bonus_mode_opens_on_bonuses() {
    PanelFixture f;
    f.results.match_kind = A11Y_RESULTS_BONUS;
    assert(f.open(A11Y_RESULTS_PAGE_BONUSES) ==
           std::string("Bonuses. Switzerland, 12000.") + kStatsKeys);
}

void kos_and_falls_rows_and_their_edges() {
    PanelFixture f;
    f.open();
    /* Up at the first row: the row again. */
    assert(f.press(kUp) == "KOs");
    std::vector<std::string> rows;
    for (int i = 0; i < 9; i++) {
        rows.push_back(f.press(kDown));
    }
    assert((rows == std::vector<std::string>{"P2 Mario, 1", "P3 Fox, 0", "P4 DK, 0", "Falls",
                        "P2 Mario, 0", "P3 Fox, 0", "P4 DK, 1", "SDs, 0", "SDs, 0"}));
    assert(f.press(kUp) == "P4 DK, 1");
}

void an_opponent_row_names_you_and_name_tags() {
    PanelFixture f;
    /* Mario's tag, full-width "MA" in Shift-JIS, in place of his port. */
    for (int port : {0, 2, 3}) {
        for (A11yResultsOpponent& opponent : f.results.players[port].opponents) {
            if (opponent.port == 1) {
                std::strcpy(opponent.tag, "\x82\x6c\x82\x60");
            }
        }
    }
    f.open();
    assert(f.press(kDown) == "MA Mario, 1");
    /* On Mario's list, you are you. */
    assert(f.press(kRight) == "Mario, P2, P1 Link, you, 0");
}

void stats_rows_in_the_screen_order() {
    PanelFixture f;
    f.results.players[0].stats[21] = -1;
    f.open(A11Y_RESULTS_PAGE_STATS);
    std::vector<std::string> rows;
    for (int i = 0; i < 23; i++) {
        rows.push_back(f.press(kDown));
    }
    assert((rows ==
            std::vector<std::string>{"Falls, 1", "SDs, 0", "Damage given, 87", "Damage taken, 0",
                "Damage recovered, 0", "Peak damage, 0", "Flight distance, 0", "Ground time, 0",
                "Air time, 0", "Hit percentage, 0", "Ground attacks, 0", "Air attacks, 0",
                "Smash attacks, 0", "Attempted grabs, 3", "Throws, 0", "Cliffhangers, 0",
                "Items thrown, 0", "Items grabbed, 0", "Top speed, 0", "Fastest pitch, 0",
                "Longest drought, none", "Transformation time, 0", "Transformation time, 0"}));
}

void bonus_rows_and_an_empty_page() {
    PanelFixture f;
    f.open(A11Y_RESULTS_PAGE_BONUSES);
    for (int i = 0; i < 3; i++) {
        f.press(kDown);
    }
    assert(f.press(kDown) == "Last Place, -1000");
    assert(f.press(kDown) == "Last Place, -1000");
    /* Fox earned none. */
    f.press(kRight);
    assert(f.press(kRight) == "Fox, P3, No bonuses");
    assert(f.press(kDown) == "No bonuses");
    /* The row number kept from before Fox. */
    assert(f.press(kLeft) == "Mario, P2, Ambler, 2500");
    f.results.players[0].page = A11Y_RESULTS_PAGE_STATS;
    f.press(kNone);
    f.press(kRight);
    assert(f.page(A11Y_RESULTS_PAGE_BONUSES) == "Bonuses. No bonuses.");
}

/* The spec's table: the cursor on your stats (Link, P1), right to Mario. */
void switching_says_only_what_differs() {
    {
        PanelFixture f;
        f.open(A11Y_RESULTS_PAGE_STATS);
        for (int i = 0; i < 3; i++) {
            f.press(kDown);
        }
        assert(f.press(kNone).empty());
        assert(f.press(kRight) == "Mario, P2, 41");
    }
    {
        /* "P3 Fox, 0" on both lists. */
        PanelFixture f;
        f.open();
        f.press(kDown);
        assert(f.press(kDown) == "P3 Fox, 0");
        assert(f.press(kRight) == "Mario, P2, 0");
    }
    {
        /* Mario's row there is P1. */
        PanelFixture f;
        f.open();
        assert(f.press(kDown) == "P2 Mario, 1");
        assert(f.press(kRight) == "Mario, P2, P1 Link, you, 0");
    }
    {
        /* A heading is its whole content. */
        PanelFixture f;
        f.open();
        assert(f.press(kRight) == "Mario, P2, KOs");
    }
    {
        PanelFixture f;
        f.results.players[1].page_sds = 1;
        f.open();
        for (int i = 0; i < 8; i++) {
            f.press(kDown);
        }
        assert(f.press(kRight) == "Mario, P2, 1");
    }
    {
        PanelFixture f;
        f.open(A11Y_RESULTS_PAGE_BONUSES);
        assert(f.press(kRight) == "Mario, P2, Stiff Knees, 300");
    }
    {
        /* Row 5, where Mario has 3: his last. */
        PanelFixture f;
        f.open(A11Y_RESULTS_PAGE_BONUSES);
        for (int i = 0; i < 4; i++) {
            f.press(kDown);
        }
        assert(f.press(kRight) == "Mario, P2, Ambler, 2500");
        /* The row number stays clamped back. */
        assert(f.press(kLeft) == "Link, P1, you, Throw Down, 1000");
    }
}

void player_order_and_edges() {
    PanelFixture f;
    f.open(A11Y_RESULTS_PAGE_STATS);
    /* You first, at the left edge: you again. */
    assert(f.press(kLeft) == "Link, P1, you");
    assert(f.press(kRight) == "Mario, P2, 0");
    assert(f.press(kRight) == "Fox, P3, 0");
    assert(f.press(kRight) == "DK, P4, 2");
    assert(f.press(kRight) == "DK, P4");
    assert(f.press(kUp) == "KOs, 2");

    /* Online you are P2: you first, then the others by port. */
    PanelFixture g;
    g.results.players[1].slot = A11Y_RESULTS_HUMAN;
    g.results.local_port = 1;
    g.open(A11Y_RESULTS_PAGE_STATS);
    assert(g.press(kLeft) == "Mario, P2, you");
    assert(g.press(kRight) == "Link, P1, 1");
    assert(g.press(kRight) == "Fox, P3, 0");

    /* Nobody is you: from P1 by port; empty ports skipped. */
    PanelFixture h;
    h.results.players[0].slot = A11Y_RESULTS_CPU;
    h.results.players[2] = A11yResultsPlayer{};
    h.open(A11Y_RESULTS_PAGE_STATS);
    assert(h.press(kLeft) == "Link, P1");
    assert(h.press(kRight) == "Mario, P2, 0");
    assert(h.press(kRight) == "DK, P4, 2");
}

void z_says_the_players_summary_line() {
    PanelFixture f;
    f.open();
    assert(f.press(kZ) == "Link, P1, you, 2nd, 1 KO, 1 fall, 0 SDs, KO'd at 1:46.");
    f.press(kRight);
    assert(f.press(kZ) == "Mario, P2, 4th, 0 KOs, 1 fall, 0 SDs, KO'd at 1:16.");
    f.press(kRight);
    f.press(kRight);
    assert(f.press(kZ) == "DK, P4, 1st, 2 KOs, 0 falls, 0 SDs, not KO'd.");

    /* No place on a cancelled match; the team in a team match. */
    PanelFixture g;
    g.results.cancelled = true;
    g.open();
    assert(g.press(kZ) == "Link, P1, you, 1 KO, 1 fall, 0 SDs, KO'd at 1:46.");
    PanelFixture h;
    h.results.teams = true;
    h.results.players[0].team = 0;
    h.open();
    assert(h.press(kZ) == "Link, P1, you, 2nd, Red team, 1 KO, 1 fall, 0 SDs, KO'd at 1:46.");
}

void the_cursor_works_while_you_are_ready() {
    PanelFixture f;
    f.open();
    f.results.players[0].ready = true;
    assert(f.press(kDown) == "P2 Mario, 1");
    assert(f.press(kNone).empty());
}

/* The spec's example with humans on the ports given besides yours, P1,
 * the panels open and taking input: the CPUs readied by the game, said
 * nothing of. */
struct ReadyFixture : PanelFixture {
    explicit ReadyFixture(std::initializer_list<int> humans) {
        for (int port : humans) {
            results.players[port].slot = A11Y_RESULTS_HUMAN;
        }
        open();
        for (A11yResultsPlayer& player : results.players) {
            player.ready = player.slot == A11Y_RESULTS_CPU;
        }
        assert(tick(results, A11Y_RESULTS_STATS).empty());
    }

    /* The port's ready flag set, seen by the next tick in the state given;
     * returns what it said. */
    Outputs ready(int port, bool ready, A11yResultsState state = A11Y_RESULTS_STATS) {
        results.players[port].ready = ready;
        return tick(results, state);
    }
};

Outputs interrupting(const char* text) {
    return Outputs{Output{text, true}};
}

Outputs queued(const char* text) {
    return Outputs{Output{text, false}};
}

void your_ready_names_every_human_you_wait_for() {
    ReadyFixture one{1};
    assert(one.ready(0, true) == interrupting("Ready, waiting for P2."));
    ReadyFixture two{1, 2};
    assert(two.ready(0, true) == interrupting("Ready, waiting for P2 and P3."));
    ReadyFixture three{1, 2, 3};
    assert(three.ready(0, true) == interrupting("Ready, waiting for P2, P3 and P4."));
    /* A human already ready is not waited for. */
    ReadyFixture readied{1, 3};
    assert(readied.ready(3, true) == queued("P4 ready."));
    assert(readied.ready(0, true) == interrupting("Ready, waiting for P2."));
}

void a_second_start_says_not_ready() {
    ReadyFixture f{1};
    f.ready(0, true);
    assert(f.ready(0, false) == interrupting("Not ready."));
    assert(f.ready(0, true) == interrupting("Ready, waiting for P2."));
}

void with_one_human_your_ready_says_nothing() {
    /* The scene leaves on the tick you ready. */
    ReadyFixture f{};
    assert(f.ready(0, true, A11Y_RESULTS_LEAVING).empty());
    /* A second human without a controller, readied by the game as the
     * panels take input, is said nothing of and not waited for. */
    PanelFixture g;
    g.results.players[1].slot = A11Y_RESULTS_HUMAN;
    g.results.players[1].unplugged = true;
    g.open();
    for (A11yResultsPlayer& player : g.results.players) {
        player.ready = player.slot == A11Y_RESULTS_CPU || player.unplugged;
    }
    assert(g.tick(g.results, A11Y_RESULTS_STATS).empty());
    g.results.players[0].ready = true;
    assert(g.tick(g.results, A11Y_RESULTS_LEAVING).empty());
}

void another_humans_ready_and_unready_are_queued() {
    ReadyFixture f{1, 2};
    assert(f.ready(1, true) == queued("P2 ready."));
    assert(f.ready(1, false) == queued("P2 not ready."));
    /* Yours first, then theirs, on one tick. */
    f.results.players[2].ready = true;
    Outputs said = f.ready(0, true);
    assert((said == Outputs{{"Ready, waiting for P2.", true}, {"P3 ready.", false}}));
    /* The last ready ends the scene: nothing is said. */
    assert(f.ready(1, true, A11Y_RESULTS_LEAVING).empty());
}

void online_the_other_human_is_your_opponent() {
    /* You are P2, the local player; P1 is your opponent. */
    PanelFixture f;
    f.results.online = true;
    f.results.local_port = 1;
    f.results.players[0].slot = A11Y_RESULTS_HUMAN;
    f.results.players[1].slot = A11Y_RESULTS_HUMAN;
    f.results.players[2].slot = A11Y_RESULTS_EMPTY;
    f.results.players[3].slot = A11Y_RESULTS_EMPTY;
    f.open();
    for (int port : {2, 3}) {
        f.results.players[port].ready = true;
        f.results.players[port].unplugged = true;
    }
    assert(f.tick(f.results, A11Y_RESULTS_STATS).empty());
    f.results.players[1].ready = true;
    assert(
        f.tick(f.results, A11Y_RESULTS_STATS) == interrupting("Ready, waiting for your opponent."));
    f.results.players[1].ready = false;
    assert(f.tick(f.results, A11Y_RESULTS_STATS) == interrupting("Not ready."));
    f.results.players[0].ready = true;
    assert(f.tick(f.results, A11Y_RESULTS_STATS) == queued("Your opponent is ready."));
    f.results.players[0].ready = false;
    assert(f.tick(f.results, A11Y_RESULTS_STATS) == queued("Your opponent is not ready."));
}

void ready_keeps_the_reading_cursor() {
    ReadyFixture f{1};
    assert(f.press(kDown) == "P2 Mario, 1");
    f.ready(0, true);
    assert(f.press(kDown) == "P3 Fox, 0");
    f.ready(0, false);
    assert(f.press(kRight) == "Mario, P2, 0");
    /* A new visit starts with nobody's ready seen. */
    f.results.players[0].ready = false;
    f.results.players[1].ready = false;
    f.open();
    assert(f.ready(1, true) == queued("P2 ready."));
}

void the_dpad_is_the_cursors_while_speech_is_on() {
    Fixture on;
    assert(on.reader.takes_dpad());
    Outputs outputs;
    a11y::Speech speech{a11y::Config{false, true}, std::make_unique<FakeBridge>(outputs)};
    speech.init();
    a11y::ResultsSpeech off{speech, a11y::GameTextSource{}};
    assert(!off.takes_dpad());
}

void an_unknown_label_is_said_by_number_and_logged_once() {
    PanelFixture f;
    f.results.stat_labels[3][0] = nullptr;
    f.results.stat_labels[3][1] = nullptr;
    f.results.players[0].bonuses[0].name = nullptr;
    f.results.kos_heading = nullptr;
    f.open(A11Y_RESULTS_PAGE_STATS);
    for (int i = 0; i < 3; i++) {
        f.press(kDown);
    }
    assert(f.press(kNone).empty());
    assert(f.press(kUp) == "SDs, 0");
    assert(f.press(kDown) == "Stat 4, 87");
    assert(f.press(kUp) == "SDs, 0");
    assert(f.press(kDown) == "Stat 4, 87");
    assert(count_logged("[a11y] results: stat 4 has no label") == 1);
    assert(f.page(A11Y_RESULTS_PAGE_BONUSES) == "Bonuses. Bonus 40, 12000.");
    assert(count_logged("[a11y] results: no game text for \"Bonus 40\"") == 1);
    /* A heading the table lacks: the fork's own word. */
    assert(f.page(A11Y_RESULTS_PAGE_STATS) == "Stats. KOs, 1.");
    assert(f.page(A11Y_RESULTS_PAGE_KOS) == "KOs and falls. KOs.");
    assert(count_logged("[a11y] results: no game text for \"KOs\"") == 1);
}

void stat_labels_are_spaced() {
    assert(a11y::stat_label("DamageGiven") == "Damage given");
    assert(a11y::stat_label("HitPercentage") == "Hit percentage");
    assert(a11y::stat_label("Attempted Grabs") == "Attempted grabs");
    assert(a11y::stat_label("KOs") == "KOs");
    assert(a11y::stat_label("Cliffhangers") == "Cliffhangers");
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
    stats_open_with_the_page_and_its_first_row();
    the_opening_press_reads_no_row();
    stats_opened_by_another_player_and_keys_once_a_run();
    stats_opening_queues_behind_an_opening_said_on_the_same_tick();
    page_changes_and_their_silent_ends();
    a_page_change_goes_to_its_first_row_and_keeps_the_player();
    bonus_mode_opens_on_bonuses();
    kos_and_falls_rows_and_their_edges();
    an_opponent_row_names_you_and_name_tags();
    stats_rows_in_the_screen_order();
    bonus_rows_and_an_empty_page();
    switching_says_only_what_differs();
    player_order_and_edges();
    z_says_the_players_summary_line();
    the_cursor_works_while_you_are_ready();
    your_ready_names_every_human_you_wait_for();
    a_second_start_says_not_ready();
    with_one_human_your_ready_says_nothing();
    another_humans_ready_and_unready_are_queued();
    online_the_other_human_is_your_opponent();
    ready_keeps_the_reading_cursor();
    the_dpad_is_the_cursors_while_speech_is_on();
    an_unknown_label_is_said_by_number_and_logged_once();
    stat_labels_are_spaced();
    std::cout << "results_speech: all tests passed\n";
    return 0;
}
