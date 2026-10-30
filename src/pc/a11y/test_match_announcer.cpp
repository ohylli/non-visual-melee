/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The match announcements against hand-written HUD snapshots and a fake
 * clock, speech over a fake screen reader with fake voices that record each
 * line, and the rows in a value model. Links match_announcer.cpp,
 * match_announcement_settings.cpp, value_model.cpp, game_text.cpp and
 * speech.cpp only, so this file supplies the pc_log_line that main.c
 * normally provides. */
#include "match_announcement_settings.hpp"
#include "match_announcer.hpp"
#include "speech.hpp"
#include "value_model.hpp"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<std::string> s_lines;

bool logged(const std::string& line) {
    return std::find(s_lines.begin(), s_lines.end(), line) != s_lines.end();
}

class FakeBridge final : public a11y::ScreenReaderBridge {
public:
    std::string init() override { return "Fake"; }
    void shutdown() override {}
    std::string output(std::string_view text, bool interrupt) override {
        spoken.push_back((interrupt ? "interrupt " : "queue ") + std::string(text));
        return "";
    }
    /* Each line given to the screen reader, as "<interrupt|queue> <text>". */
    std::vector<std::string> spoken;
};

/* The voices as speech sees them: each line as "<voice> <subject> <text>".
 * While a line plays (playing), the next lines wait, and one about a subject
 * whose line is waiting takes that line's place, as voices.hpp queues them. */
class FakeVoices final : public a11y::VoiceLines {
public:
    void start() override {}
    void say(a11y::Voice voice, a11y::Subject subject, std::string_view text) override {
        const std::string about =
            std::string(a11y::voice_name(voice)) + " " + std::to_string(subject) + " ";
        if (!playing) {
            said.push_back(about + std::string(text));
            return;
        }
        for (std::string& line : waiting) {
            if (line.compare(0, about.size(), about) == 0) {
                line = about + std::string(text);
                return;
            }
        }
        waiting.push_back(about + std::string(text));
    }
    void drop_waiting(const char*) override {}

    std::vector<std::string> said;
    bool playing = false;
    std::vector<std::string> waiting;
};

using Lines = std::vector<std::string>;

/* A human in slot 0 on port 1 (you) against a CPU in slot 1, both with a
 * panel, before GO. */
A11yMatchHud before_go() {
    A11yMatchHud hud{};
    hud.match = true;
    hud.hud_enabled = false;
    hud.decided = false;
    hud.shows_stocks = true;
    hud.you = 0;
    for (int slot = 0; slot < 2; slot++) {
        A11yMatchHudPlayer& player = hud.players[slot];
        player.present = true;
        player.panel = true;
        player.port = slot;
        player.stocks = 4;
    }
    hud.players[0].human = true;
    return hud;
}

/* A player added to a snapshot, with a panel. */
void add(A11yMatchHud& hud, int slot, bool human, int port, const char* tag = "") {
    A11yMatchHudPlayer& player = hud.players[slot];
    player = A11yMatchHudPlayer{};
    player.present = true;
    player.panel = true;
    player.human = human;
    player.port = port;
    player.stocks = 4;
    std::strncpy(player.tag, tag, sizeof(player.tag) - 1);
}

struct Fixture {
    FakeVoices voices;
    std::unique_ptr<FakeBridge> made = std::make_unique<FakeBridge>();
    FakeBridge* bridge = made.get();
    a11y::Speech speech{a11y::Config{}, std::move(made)};
    a11y::MatchAnnouncer announcer{speech};
    a11y::MatchAnnouncementValues values;
    A11yMatchHud hud = before_go();
    /* The fake clock: a tick is a sixtieth of a second. */
    static constexpr std::chrono::microseconds kTick{16667};
    a11y::Clock::time_point now = a11y::Clock::time_point{} + std::chrono::hours(1);

    Fixture() {
        speech.init();
        speech.attach_voices(&voices);
        announcer.scene_entered();
    }

    /* Runs n ticks of the HUD as it stands and returns what the voices were
     * given, clearing it. */
    Lines ticks(int n = 1) {
        for (int i = 0; i < n; i++) {
            now += kTick;
            announcer.tick(hud, values, now);
        }
        Lines said = std::move(voices.said);
        voices.said.clear();
        return said;
    }

    /* Ticks until a figure changed now has settled. */
    Lines settle() { return ticks(static_cast<int>(values.settle / kTick) + 2); }

    /* The HUD comes on: GO. */
    Lines go() {
        hud.hud_enabled = true;
        return ticks();
    }

    /* A timer on the HUD, counting down unless up, as it stands at the
     * match's start: a countdown from seconds shows "M:SS.00". */
    void timer(int seconds, bool up = false) {
        hud.timer = true;
        hud.timer_counts_up = up;
        hud.timer_seconds = seconds;
        hud.timer_frames = up ? 0 : 59;
    }

    /* Runs n ticks with the timer advancing first, as the game's does
     * (fn_8016CD98), and returns what the screen reader was given, clearing
     * it. at, if given, gets the timer's figures on each line's tick. */
    Lines count(int n, std::vector<std::pair<int, int>>* at = nullptr) {
        Lines spoken;
        for (int i = 0; i < n; i++) {
            if (hud.timer && hud.hud_enabled && ++hud.timer_frames >= 60) {
                hud.timer_frames = 0;
                if (hud.timer_counts_up) {
                    hud.timer_seconds++;
                } else if (hud.timer_seconds != 0) {
                    hud.timer_seconds--;
                }
            }
            ticks();
            for (std::string& line : bridge->spoken) {
                if (at != nullptr) {
                    at->emplace_back(hud.timer_seconds, hud.timer_frames);
                }
                spoken.push_back(std::move(line));
            }
            bridge->spoken.clear();
        }
        return spoken;
    }
};

/* A combo's hits come closer together than the settle time: only where it
 * ends is said, once it has held still for the settle time. */
void settling_through_a_combo() {
    Fixture f;
    assert(f.go().empty());
    const int gap = static_cast<int>(f.values.settle / Fixture::kTick) / 2;
    for (int damage : {9, 17, 26, 40}) {
        f.hud.players[1].damage = damage;
        assert(f.ticks(gap).empty());
    }
    /* No maximum wait: the last hit restarted the wait. */
    assert(f.ticks(gap - 1).empty());
    assert((f.settle() == Lines{"opponents 1 40"}));
    assert(f.settle().empty());
}

/* Your damage in your voice; a fall is said as a rise is. */
void a_rise_and_a_fall() {
    Fixture f;
    f.go();
    f.hud.players[0].damage = 15;
    assert((f.settle() == Lines{"yours 0 15"}));
    f.hud.players[0].damage = 52;
    assert((f.settle() == Lines{"yours 0 52"}));
    /* PSI Magnet, Training's menu: down is said too. */
    f.hud.players[0].damage = 30;
    assert((f.settle() == Lines{"yours 0 30"}));
    /* Back to the figure last said before it settled: nothing. */
    f.hud.players[0].damage = 41;
    f.ticks(3);
    f.hud.players[0].damage = 30;
    assert(f.settle().empty());
}

/* A healing item takes a point a tick: the run is said once, at its end. */
void a_heal_is_said_once() {
    Fixture f;
    f.go();
    f.hud.players[1].damage = 80;
    assert((f.settle() == Lines{"opponents 1 80"}));
    Lines said;
    for (int damage = 79; damage >= 50; damage--) {
        f.hud.players[1].damage = damage;
        Lines tick = f.ticks();
        said.insert(said.end(), tick.begin(), tick.end());
    }
    assert(said.empty());
    assert((f.settle() == Lines{"opponents 1 50"}));
}

/* A KO resets the last figure to 0 silently, so the respawn's 0% is never
 * said, whether the KO zeroes the damage on its tick or at the end of a
 * star KO's flight; the next hit is said from 0. */
void a_ko_resets_silently() {
    Fixture f;
    f.go();
    f.hud.players[1].damage = 85;
    assert((f.settle() == Lines{"opponents 1 85"}));
    /* A star KO: out with the damage still showing, then zeroed. */
    f.hud.players[1].out = true;
    assert(f.settle().empty());
    f.hud.players[1].damage = 0;
    assert(f.settle().empty());
    f.hud.players[1].out = false;
    assert(f.settle().empty());
    /* A side KO: out and zeroed on one tick, a figure still settling
     * dropped with it. */
    f.hud.players[1].damage = 30;
    f.ticks(2);
    f.hud.players[1].out = true;
    f.hud.players[1].damage = 0;
    assert(f.settle().empty());
    f.hud.players[1].out = false;
    assert(f.settle().empty());
    f.hud.players[1].damage = 12;
    assert((f.settle() == Lines{"opponents 1 12"}));
}

/* Each damage at GO is taken as said: sudden death's 300% says nothing
 * until it changes, nor does anything before GO. */
void the_baseline_at_go() {
    Fixture f;
    f.hud.players[0].damage = 300;
    f.hud.players[1].damage = 300;
    assert(f.settle().empty());
    assert(f.go().empty());
    assert(f.settle().empty());
    f.hud.players[1].damage = 313;
    assert((f.settle() == Lines{"opponents 1 313"}));

    /* A change before GO is not said either. */
    Fixture g;
    g.hud.players[1].damage = 20;
    g.ticks(3);
    g.hud.players[1].damage = 40;
    g.ticks(3);
    assert(g.go().empty());
    assert(g.settle().empty());
}

/* At GAME! or TIME! a figure still settling is dropped and logged as
 * dropped. Online a rollback can take GAME! back: the match goes on,
 * followed afresh from what the HUD shows. A match left from the pause is
 * gone until the next scene. */
void game_drops_a_pending_figure() {
    Fixture f;
    f.go();
    f.hud.players[0].damage = 45;
    f.hud.players[1].damage = 70;
    f.ticks(3);
    f.hud.decided = true;
    assert(f.settle().empty());
    assert(logged("[a11y] match announcement yours dropped (match over): \"45\""));
    assert(logged("[a11y] match announcement opponents dropped (match over): \"70\""));
    f.hud.players[1].damage = 90;
    assert(f.settle().empty());
    f.hud.decided = false;
    assert(f.settle().empty());
    f.hud.players[1].damage = 95;
    assert((f.settle() == Lines{"opponents 1 95"}));

    /* The loser's star KO, cut short by the freeze, is no figure owed. */
    Fixture g;
    g.go();
    g.hud.players[1].damage = 120;
    g.settle();
    s_lines.clear();
    g.hud.players[1].out = true;
    g.ticks();
    g.hud.decided = true;
    g.ticks();
    assert(s_lines.empty());

    /* Left from the pause: no GAME!, the fighters gone with the scene. */
    Fixture h;
    h.go();
    h.hud.players[1].damage = 33;
    h.ticks();
    h.hud = A11yMatchHud{};
    assert(h.settle().empty());
    assert(logged("[a11y] match announcement opponents dropped (match gone): \"33\""));
    h.hud = before_go();
    h.hud.hud_enabled = true;
    h.ticks(3);
    h.hud.players[1].damage = 40;
    assert(h.settle().empty());

    /* The next scene's match starts anew. */
    h.announcer.scene_entered();
    h.hud = before_go();
    h.hud.players[1].damage = 33;
    assert(h.go().empty());
    h.hud.players[1].damage = 50;
    assert((h.settle() == Lines{"opponents 1 50"}));
}

/* Labels: none for one opponent; with two or more players besides you each
 * line in the opponents' voice starts with the player's: the name tag for a
 * human with one, offline only, else "P" and the port, "CPU" and the slot
 * for a computer player, "teammate" before a teammate's. Yours never has
 * one. */
void labels() {
    Fixture one;
    one.go();
    one.hud.players[1].damage = 45;
    assert((one.settle() == Lines{"opponents 1 45"}));

    Fixture three;
    add(three.hud, 2, true, 2);
    add(three.hud, 3, true, 3, "BOB");
    three.go();
    three.hud.players[0].damage = 10;
    three.hud.players[1].damage = 20;
    three.hud.players[2].damage = 30;
    three.hud.players[3].damage = 40;
    assert((three.settle() == Lines{"yours 0 10", "opponents 1 CPU 2 20", "opponents 2 P3 30",
                                  "opponents 3 BOB 40"}));

    /* Online the tag is the peer's memory card's: the port instead. */
    Fixture online;
    online.hud.online = true;
    add(online.hud, 2, true, 2, "BOB");
    online.go();
    online.hud.players[2].damage = 30;
    assert((online.settle() == Lines{"opponents 2 P3 30"}));

    /* Teams: you and a CPU against a human. */
    Fixture teams;
    teams.hud.teams = true;
    add(teams.hud, 2, true, 2);
    teams.hud.players[0].team = 1;
    teams.hud.players[1].team = 1;
    teams.hud.players[2].team = 0;
    teams.go();
    teams.hud.players[1].damage = 12;
    teams.hud.players[2].damage = 24;
    assert((teams.settle() == Lines{"opponents 1 teammate CPU 2 12", "opponents 2 P3 24"}));

    /* A computer player goes by its slot, its port whatever it is. */
    Fixture cpu;
    add(cpu.hud, 2, false, 0);
    cpu.go();
    cpu.hud.players[2].damage = 5;
    assert((cpu.settle() == Lines{"opponents 2 CPU 3 5"}));
}

/* No "you" (four computer players): everyone in the opponents' voice,
 * labelled. */
void no_you() {
    Fixture f;
    f.hud.you = -1;
    f.hud.players[0].human = false;
    f.go();
    f.hud.players[0].damage = 8;
    f.hud.players[1].damage = 16;
    assert((f.settle() == Lines{"opponents 0 CPU 1 8", "opponents 1 CPU 2 16"}));
}

/* The labels are decided at GO and kept: an elimination leaves them, and a
 * player who joins later is not labelled when GO had one opponent. */
void labels_are_fixed_at_go() {
    Fixture f;
    add(f.hud, 2, false, 2);
    f.go();
    f.hud.players[2].panel = false;
    f.hud.players[1].damage = 20;
    assert((f.settle() == Lines{"opponents 1 CPU 2 20"}));

    Fixture g;
    g.go();
    add(g.hud, 2, false, 2);
    g.hud.players[2].damage = 7;
    /* The newcomer's figure on arrival is its baseline. */
    assert(g.settle().empty());
    g.hud.players[2].damage = 19;
    g.hud.players[1].damage = 3;
    assert((g.settle() == Lines{"opponents 1 3", "opponents 2 19"}));
}

/* A fighter without a HUD panel (Multi-Man's wireframes) is never
 * announced, nor a stamina player, whose HUD shows HP. */
void no_panel_no_announcement() {
    Fixture f;
    add(f.hud, 2, false, 2);
    f.hud.players[2].panel = false;
    f.go();
    f.hud.players[2].damage = 50;
    assert(f.settle().empty());
    /* It counted for no label either. */
    f.hud.players[1].damage = 9;
    assert((f.settle() == Lines{"opponents 1 9"}));

    Fixture stamina;
    stamina.hud.players[1].stamina = true;
    stamina.go();
    stamina.hud.players[1].damage = 30;
    assert(stamina.settle().empty());
}

/* The HUD hidden mid-match (the pause camera): a figure or a stock count
 * waits until it shows again. */
void hidden_hud_holds() {
    Fixture f;
    f.go();
    f.hud.players[1].damage = 25;
    f.hud.hud_enabled = false;
    assert(f.settle().empty());
    f.hud.hud_enabled = true;
    assert((f.ticks() == Lines{"opponents 1 25"}));
    /* A stock change too. */
    f.hud.hud_enabled = false;
    f.hud.players[1].stocks = 3;
    assert(f.settle().empty());
    f.hud.hud_enabled = true;
    assert((f.ticks() == Lines{"opponents 1 3 stocks"}));
}

/* The Match announcements switch: off, nothing is said and every figure is
 * taken as said, so turned on again only later changes are said. Each
 * side's damage off says nothing for that side. */
void the_switch() {
    Fixture f;
    f.go();
    f.values.on = false;
    f.hud.players[0].damage = 30;
    f.hud.players[1].damage = 40;
    assert(f.settle().empty());
    f.values.on = true;
    assert(f.settle().empty());
    f.hud.players[1].damage = 55;
    assert((f.settle() == Lines{"opponents 1 55"}));
    /* Switched off while a figure settles: dropped silently. */
    f.hud.players[1].damage = 60;
    f.ticks(2);
    f.values.on = false;
    f.ticks();
    f.values.on = true;
    assert(f.settle().empty());

    f.values.your_damage = a11y::DamageMode::off;
    f.hud.players[0].damage = 44;
    f.hud.players[1].damage = 66;
    assert((f.settle() == Lines{"opponents 1 66"}));
    f.values.your_damage = a11y::DamageMode::every_change;
    f.values.opponents_damage = a11y::DamageMode::off;
    f.hud.players[0].damage = 45;
    f.hud.players[1].damage = 67;
    assert((f.settle() == Lines{"yours 0 45"}));

    /* With Speech off a line is only logged, marked (off). */
    f.values.opponents_damage = a11y::DamageMode::every_change;
    f.speech.set_on(false);
    f.hud.players[1].damage = 80;
    assert(f.settle().empty());
    assert(logged("[a11y] voice opponents (off): \"80\""));
}

/* Thresholds: the settled damage is said only once it has crossed a
 * multiple of the interval upward since the last figure said, however many
 * at once; a rise that crosses none is silent and leaves the figure the next
 * crossing counts from. Each side has its own mode. */
void thresholds_crossings() {
    for (const bool yours : {true, false}) {
        Fixture f;
        f.values.your_damage =
            yours ? a11y::DamageMode::thresholds : a11y::DamageMode::every_change;
        f.values.opponents_damage =
            yours ? a11y::DamageMode::every_change : a11y::DamageMode::thresholds;
        const int slot = yours ? 0 : 1;
        const std::string voice = yours ? "yours 0 " : "opponents 1 ";
        f.go();
        /* Up to 35 crosses 20: said. */
        f.hud.players[slot].damage = 35;
        assert((f.settle() == Lines{voice + "35"}));
        /* 38 crosses nothing. */
        f.hud.players[slot].damage = 38;
        assert(f.settle().empty());
        /* 52 crosses 40, counted from 35. */
        f.hud.players[slot].damage = 52;
        assert((f.settle() == Lines{voice + "52"}));
        /* Several at once, said once: 52 to 85 crosses 60 and 80. */
        f.hud.players[slot].damage = 85;
        assert((f.settle() == Lines{voice + "85"}));
        /* Reaching a multiple crosses it. */
        f.hud.players[slot].damage = 100;
        assert((f.settle() == Lines{voice + "100"}));
        /* The other side still says every change. */
        f.hud.players[1 - slot].damage = 3;
        assert(f.settle().size() == 1);
    }
}

/* A fall is silent but lowers the figure the next crossing counts from: 85,
 * healed to 55, hit to 65 says "65". A rise above the fall that crosses no
 * multiple stays silent. */
void thresholds_a_fall_then_a_crossing() {
    Fixture f;
    f.values.opponents_damage = a11y::DamageMode::thresholds;
    f.go();
    f.hud.players[1].damage = 85;
    assert((f.settle() == Lines{"opponents 1 85"}));
    for (int damage = 84; damage >= 55; damage--) {
        f.hud.players[1].damage = damage;
        assert(f.ticks().empty());
    }
    assert(f.settle().empty());
    f.hud.players[1].damage = 58;
    assert(f.settle().empty());
    f.hud.players[1].damage = 65;
    assert((f.settle() == Lines{"opponents 1 65"}));
}

/* A KO resets the figure to 0, silently: the next hit is said only once it
 * crosses the first multiple. */
void thresholds_a_ko() {
    Fixture f;
    f.values.opponents_damage = a11y::DamageMode::thresholds;
    f.go();
    f.hud.players[1].damage = 75;
    assert((f.settle() == Lines{"opponents 1 75"}));
    f.hud.players[1].out = true;
    f.hud.players[1].damage = 0;
    assert(f.settle().empty());
    f.hud.players[1].out = false;
    assert(f.settle().empty());
    f.hud.players[1].damage = 15;
    assert(f.settle().empty());
    f.hud.players[1].damage = 24;
    assert((f.settle() == Lines{"opponents 1 24"}));
}

/* A change of interval or mode takes effect at once, partway through a
 * match: the crossing counts from the last figure said, at the interval in
 * force. */
void thresholds_changes_take_effect_at_once() {
    Fixture f;
    f.values.opponents_damage = a11y::DamageMode::thresholds;
    f.go();
    f.hud.players[1].damage = 21;
    assert((f.settle() == Lines{"opponents 1 21"}));
    /* At 20, 21 to 35 crosses nothing; at 10 it crosses 30. */
    f.hud.players[1].damage = 35;
    assert(f.settle().empty());
    f.values.damage_interval = 10;
    assert((f.settle() == Lines{"opponents 1 35"}));
    /* At 50, 35 to 48 crosses nothing, 35 to 50 crosses 50. */
    f.values.damage_interval = 50;
    f.hud.players[1].damage = 48;
    assert(f.settle().empty());
    f.hud.players[1].damage = 50;
    assert((f.settle() == Lines{"opponents 1 50"}));
    /* To every change: 50 to 53 is said; back to thresholds, 53 to 58 is
     * not. */
    f.values.opponents_damage = a11y::DamageMode::every_change;
    f.hud.players[1].damage = 53;
    assert((f.settle() == Lines{"opponents 1 53"}));
    f.values.opponents_damage = a11y::DamageMode::thresholds;
    f.hud.players[1].damage = 58;
    assert(f.settle().empty());
    /* Off and on again: nothing owed from before. */
    f.values.opponents_damage = a11y::DamageMode::off;
    f.hud.players[1].damage = 120;
    assert(f.settle().empty());
    f.values.opponents_damage = a11y::DamageMode::thresholds;
    assert(f.settle().empty());
    f.hud.players[1].damage = 140;
    assert(f.settle().empty());
    f.hud.players[1].damage = 150;
    assert((f.settle() == Lines{"opponents 1 150"}));
}

/* A teammate follows the opponents' row, labelled; your own row leaves it
 * alone. */
void thresholds_a_teammate() {
    Fixture f;
    f.hud.teams = true;
    add(f.hud, 2, true, 2);
    f.hud.players[0].team = 1;
    f.hud.players[1].team = 1;
    f.hud.players[2].team = 0;
    f.values.opponents_damage = a11y::DamageMode::thresholds;
    f.go();
    f.hud.players[1].damage = 12;
    f.hud.players[2].damage = 9;
    assert(f.settle().empty());
    f.hud.players[1].damage = 27;
    assert((f.settle() == Lines{"opponents 1 teammate CPU 2 27"}));
    f.values.opponents_damage = a11y::DamageMode::off;
    f.hud.players[1].damage = 44;
    assert(f.settle().empty());
    f.values.opponents_damage = a11y::DamageMode::every_change;
    f.values.your_damage = a11y::DamageMode::off;
    f.hud.players[1].damage = 46;
    assert((f.settle() == Lines{"opponents 1 teammate CPU 2 46"}));
}

/* At GAME! a figure that crossed nothing is no figure owed; one that
 * crossed is dropped and logged. */
void thresholds_at_game() {
    Fixture f;
    f.values.your_damage = a11y::DamageMode::thresholds;
    f.values.opponents_damage = a11y::DamageMode::thresholds;
    f.go();
    f.hud.players[0].damage = 15;
    f.hud.players[1].damage = 25;
    f.ticks(3);
    s_lines.clear();
    f.hud.decided = true;
    f.ticks();
    assert((s_lines == std::vector<std::string>{
                           "[a11y] match announcement opponents dropped (match over): \"25\""}));
}

/* Stocks: said on the tick the count changes, "last stock" at one, nothing
 * at 0, where the game's announcer calls the elimination. Not at GO. */
void stocks_a_loss_and_last_stock() {
    Fixture f;
    f.hud.players[0].stocks = 3;
    f.hud.players[1].stocks = 3;
    assert(f.go().empty());
    /* A side KO: the stock goes on the KO's tick, the damage with it. */
    f.hud.players[1].damage = 60;
    f.settle();
    f.hud.players[1].out = true;
    f.hud.players[1].damage = 0;
    f.hud.players[1].stocks = 2;
    assert((f.ticks() == Lines{"opponents 1 2 stocks"}));
    assert(f.settle().empty());
    f.hud.players[1].out = false;
    f.hud.players[1].stocks = 1;
    assert((f.ticks() == Lines{"opponents 1 last stock"}));
    f.hud.players[1].stocks = 0;
    assert(f.settle().empty());
    /* Yours in your voice. */
    f.hud.players[0].stocks = 2;
    assert((f.ticks() == Lines{"yours 0 2 stocks"}));
    f.hud.players[0].stocks = 1;
    assert((f.ticks() == Lines{"yours 0 last stock"}));

    /* Labelled: after the label, with a comma. */
    Fixture three;
    add(three.hud, 2, true, 2);
    add(three.hud, 3, true, 3, "BOB");
    three.go();
    three.hud.players[1].stocks = 3;
    three.hud.players[2].stocks = 1;
    three.hud.players[3].stocks = 2;
    assert((three.ticks() == Lines{"opponents 1 CPU 2, 3 stocks", "opponents 2 P3, last stock",
                                 "opponents 3 BOB, 2 stocks"}));
}

/* A team stock steal: the eliminated player takes a stock from a teammate
 * with more than one, and both counts are said, the rise as a fall is. */
void stocks_a_steal() {
    Fixture f;
    f.hud.teams = true;
    add(f.hud, 2, false, 2);
    f.hud.players[0].team = 1;
    f.hud.players[1].team = 1;
    f.hud.players[2].team = 0;
    for (int slot = 0; slot < 3; slot++) {
        f.hud.players[slot].stocks = 3;
    }
    f.go();
    f.hud.players[0].stocks = 0;
    f.hud.players[0].out = true;
    assert(f.settle().empty());
    /* The steal, the respawn starting on its tick. */
    f.hud.players[0].stocks = 1;
    f.hud.players[0].out = false;
    f.hud.players[1].stocks = 2;
    assert((f.ticks() == Lines{"yours 0 last stock", "opponents 1 teammate CPU 2, 2 stocks"}));
    assert(f.settle().empty());
}

/* Each side's stocks switch, and the Match announcements switch: off, a
 * change is taken as said, so on again only later changes are said. */
void stocks_switches() {
    Fixture f;
    f.go();
    f.values.your_stocks = false;
    f.hud.players[0].stocks = 3;
    f.hud.players[1].stocks = 3;
    assert((f.ticks() == Lines{"opponents 1 3 stocks"}));
    f.values.your_stocks = true;
    assert(f.settle().empty());
    f.values.opponents_stocks = false;
    f.hud.players[0].stocks = 2;
    f.hud.players[1].stocks = 2;
    assert((f.ticks() == Lines{"yours 0 2 stocks"}));
    f.values.opponents_stocks = true;
    assert(f.settle().empty());
    /* Damage goes on without them. */
    f.values.your_stocks = false;
    f.values.opponents_stocks = false;
    f.hud.players[1].damage = 14;
    assert((f.settle() == Lines{"opponents 1 14"}));
    f.values.your_stocks = true;
    f.values.opponents_stocks = true;
    f.values.on = false;
    f.hud.players[0].stocks = 1;
    f.hud.players[1].stocks = 1;
    assert(f.settle().empty());
    f.values.on = true;
    assert(f.settle().empty());
}

/* A timed match shows no stocks: none said, whatever the count does. */
void stocks_not_in_a_timed_match() {
    Fixture f;
    f.hud.shows_stocks = false;
    f.go();
    f.hud.players[1].stocks = 3;
    f.hud.players[0].stocks = 1;
    assert(f.settle().empty());
}

/* The Teammates switch: off, a teammate's damage and stocks are not said,
 * and on again only later changes are; an opponent's go on. */
void the_teammates_switch() {
    Fixture f;
    f.hud.teams = true;
    add(f.hud, 2, false, 2);
    f.hud.players[0].team = 1;
    f.hud.players[1].team = 1;
    f.hud.players[2].team = 0;
    f.go();
    f.values.teammates = false;
    f.hud.players[1].damage = 30;
    f.hud.players[1].stocks = 3;
    f.hud.players[2].damage = 40;
    f.hud.players[2].stocks = 3;
    assert((f.ticks() == Lines{"opponents 2 CPU 3, 3 stocks"}));
    assert((f.settle() == Lines{"opponents 2 CPU 3 40"}));
    f.values.teammates = true;
    assert(f.settle().empty());
    f.hud.players[1].damage = 35;
    f.hud.players[1].stocks = 2;
    assert((f.ticks() == Lines{"opponents 1 teammate CPU 2, 2 stocks"}));
    assert((f.settle() == Lines{"opponents 1 teammate CPU 2 35"}));

    /* Without teams nobody is a teammate, whatever the slots hold. */
    Fixture g;
    g.hud.players[0].team = 1;
    g.hud.players[1].team = 1;
    g.values.teammates = false;
    g.go();
    g.hud.players[1].damage = 9;
    g.hud.players[1].stocks = 2;
    assert((g.ticks() == Lines{"opponents 1 2 stocks"}));
    assert((g.settle() == Lines{"opponents 1 9"}));
}

/* A stock line has the player's slot as its subject, as a damage line does,
 * so it takes the place of a damage line about the same player still
 * waiting in the voice's queue. */
void a_stock_line_replaces_a_waiting_damage_line() {
    Fixture f;
    f.go();
    f.voices.playing = true;
    f.hud.players[0].damage = 20;
    f.hud.players[1].damage = 85;
    f.settle();
    assert((f.voices.waiting == Lines{"yours 0 20", "opponents 1 85"}));
    f.hud.players[1].out = true;
    f.hud.players[1].damage = 0;
    f.hud.players[1].stocks = 3;
    f.ticks();
    assert((f.voices.waiting == Lines{"yours 0 20", "opponents 1 3 stocks"}));
}

/* A countdown says each whole minute and 30 seconds, queued on the screen
 * reader, on the tick the HUD shows "M:00.00"; the starting minute is not
 * said, nor anything in the voices. */
void time_left_each_minute() {
    Fixture f;
    f.timer(180);
    assert(f.go().empty());
    std::vector<std::pair<int, int>> at;
    assert((f.count(180 * 60, &at) ==
            Lines{"queue 2 minutes left", "queue 1 minute left", "queue 30 seconds left"}));
    assert((at == std::vector<std::pair<int, int>>{{120, 59}, {60, 59}, {30, 59}}));
    assert(f.hud.timer_seconds == 0 && f.hud.timer_frames == 59);
    assert(f.voices.said.empty());
    assert(logged("[a11y] speak queue: \"1 minute left\""));

    /* A one-minute match: only 30 seconds. */
    Fixture g;
    g.timer(60);
    g.go();
    assert((g.count(60 * 60) == Lines{"queue 30 seconds left"}));
}

/* A limit that is not a whole minute, as an event carries its time from one
 * stage to the next: the first whole minute below it is said. */
void time_left_not_a_whole_minute() {
    Fixture f;
    f.timer(103);
    f.hud.timer_frames = 20;
    f.go();
    assert((f.count(103 * 60) == Lines{"queue 1 minute left", "queue 30 seconds left"}));

    /* Set back up partway through (an event's next stage, Adventure's race):
     * the new time is where it counts from, said again below it. */
    Fixture g;
    g.timer(70);
    g.go();
    assert((g.count(10 * 60 + 1) == Lines{"queue 1 minute left"}));
    g.timer(95);
    assert((g.count(65 * 60) == Lines{"queue 1 minute left", "queue 30 seconds left"}));
}

/* A timer counting up (Multi-Man, All-Star) says nothing, nor does a match
 * without a timer, nor a countdown with no limit. */
void time_left_counting_up_and_none() {
    Fixture f;
    f.timer(0, true);
    f.go();
    assert(f.count(3 * 60 * 60).empty());
    assert(f.hud.timer_seconds == 180);

    Fixture g;
    g.go();
    assert(g.count(2 * 60 * 60).empty());

    Fixture h;
    h.timer(-1);
    h.go();
    assert(h.count(60).empty());
}

/* Paused, the timer stands still at 1:00.00: said once. Hidden by the pause
 * camera on that tick and shown again: still once. */
void time_left_a_pause() {
    Fixture f;
    f.timer(61);
    f.go();
    assert((f.count(60) == Lines{"queue 1 minute left"}));
    assert(f.hud.timer_seconds == 60 && f.hud.timer_frames == 59);
    /* Paused: the HUD on, the timer still (ticks leaves it). */
    assert(f.ticks(600).empty() && f.bridge->spoken.empty());
    /* The pause camera: the HUD hidden, the timer still. */
    f.hud.hud_enabled = false;
    assert(f.count(600).empty());
    f.hud.hud_enabled = true;
    assert(f.count(1).empty());
    assert(f.hud.timer_seconds == 59);
    assert((f.count(30 * 60) == Lines{"queue 30 seconds left"}));
}

/* The Time left switch and the Match announcements switch: off, a minute
 * passes silently, and on again only the next is said. At TIME! nothing
 * more. */
void time_left_the_switches() {
    Fixture f;
    f.timer(150);
    f.go();
    f.values.time_left = false;
    assert(f.count(31 * 60).empty());
    f.values.time_left = true;
    assert((f.count(60 * 60) == Lines{"queue 1 minute left"}));
    f.values.on = false;
    assert(f.count(30 * 60).empty());
    f.values.on = true;
    assert(f.count(25 * 60).empty());

    /* TIME!: the match is over. */
    Fixture g;
    g.timer(90);
    g.go();
    g.hud.decided = true;
    assert(g.count(90 * 60).empty());
}

/* The rows: the switch shown, on by default, read live; the settle time a
 * tuning value; the damage rows. */
void the_rows() {
    a11y::ValueModel model;
    a11y::MatchAnnouncementSettings settings(model);
    const a11y::NamedValue* on = model.find(a11y::MatchAnnouncementSettings::kOnKey);
    assert(on != nullptr && on->shown && on->kind == a11y::ValueKind::toggle);
    assert(on->label == "Match announcements" && on->default_value == 1.0f);
    assert(model.shown()[0].value == on && model.shown()[0].menu == nullptr);
    /* Then the row opening Match announcement settings, which holds the
     * rest, Voice settings first. */
    const a11y::NamedMenu& menu = settings.menu();
    assert(model.shown()[1].opens == &menu && model.shown()[1].menu == nullptr);
    assert(menu.label == "Match announcement settings" && menu.parent == nullptr);
    assert(model.shown()[2].opens == &settings.voice_menu() && model.shown()[2].menu == &menu);
    assert(
        settings.voice_menu().label == "Voice settings" && settings.voice_menu().parent == &menu);
    for (std::size_t i = 2; i < model.shown().size(); ++i) {
        assert(model.shown()[i].menu == &menu);
    }
    const a11y::NamedValue* settle = model.find("match_announcements.damage_settle");
    assert(settle != nullptr && !settle->shown);
    assert(settings.values().on);
    assert(settings.values().settle == std::chrono::duration_cast<a11y::Clock::duration>(
                                           std::chrono::duration<float>(settle->value)));
    model.set(*on, 0.0f);
    assert(!settings.values().on);
    model.set(*settle, 1.5f);
    assert(settings.values().settle == std::chrono::milliseconds(1500));

    /* The damage rows after the switch: two choices, Every change by
     * default, and the interval, a slider shared by both. */
    const a11y::NamedValue* yours = model.find(a11y::MatchAnnouncementSettings::kYourDamageKey);
    const a11y::NamedValue* theirs =
        model.find(a11y::MatchAnnouncementSettings::kOpponentsDamageKey);
    const a11y::NamedValue* interval =
        model.find(a11y::MatchAnnouncementSettings::kDamageIntervalKey);
    assert(yours != nullptr && yours->shown && yours->kind == a11y::ValueKind::choice);
    assert(theirs != nullptr && theirs->shown && theirs->kind == a11y::ValueKind::choice);
    assert(interval != nullptr && interval->shown && interval->kind == a11y::ValueKind::number);
    assert(yours->label == "Your damage" && theirs->label == "Opponents' damage");
    assert(interval->label == "Damage interval");
    assert(model.shown().size() == 10 && model.shown()[3].value == yours &&
           model.shown()[4].value == theirs && model.shown()[5].value == interval);
    const a11y::ChoiceOptions modes = a11y::choice_options(*yours);
    assert(modes.size() == 3 && modes[0].shown() == "Off" && modes[1].shown() == "Every change" &&
           modes[2].shown() == "Thresholds");
    assert(a11y::choice_options(*theirs) == modes);
    assert(yours->default_text == "Every change" && theirs->default_text == "Every change");
    assert(interval->min == 10.0f && interval->max == 100.0f && interval->step == 10.0f);
    assert(interval->default_value == 20.0f && interval->unit == "%");
    assert(settings.values().your_damage == a11y::DamageMode::every_change);
    assert(settings.values().opponents_damage == a11y::DamageMode::every_change);
    assert(settings.values().damage_interval == 20);

    /* Each read live, each side on its own. */
    model.choose(*yours, "Thresholds");
    assert(settings.values().your_damage == a11y::DamageMode::thresholds);
    assert(settings.values().opponents_damage == a11y::DamageMode::every_change);
    model.choose(*theirs, "Off");
    assert(settings.values().opponents_damage == a11y::DamageMode::off);
    /* Left from Off wraps round to Thresholds. */
    model.choose(*theirs, a11y::choice_step(*theirs, a11y::choice_options(*theirs), -1));
    assert(settings.values().opponents_damage == a11y::DamageMode::thresholds);
    model.set(*interval, 70.0f);
    assert(settings.values().damage_interval == 70);

    /* The file names the options, and reads them back. */
    const std::string text = model.to_text();
    assert(text.find("match_announcements.your_damage = Thresholds\n") != std::string::npos);
    assert(text.find("match_announcements.opponents_damage = Thresholds\n") != std::string::npos);
    assert(
        text.find("Off, Every change or Thresholds, default Every change.") != std::string::npos);
    assert(text.find("match_announcements.damage_interval = 70\n") != std::string::npos);
    a11y::ValueModel again;
    a11y::MatchAnnouncementSettings read(again);
    again.read_text(text);
    assert(read.values().your_damage == a11y::DamageMode::thresholds);
    assert(read.values().opponents_damage == a11y::DamageMode::thresholds);
    assert(read.values().damage_interval == 70);

    /* The stocks rows and Teammates after the interval: switches, on by
     * default, each read live; the settle time stays last in the file. */
    const char* const keys[] = {a11y::MatchAnnouncementSettings::kYourStocksKey,
        a11y::MatchAnnouncementSettings::kOpponentsStocksKey,
        a11y::MatchAnnouncementSettings::kTeammatesKey};
    const char* const labels[] = {"Your stocks", "Opponents' stocks", "Teammates"};
    for (int i = 0; i < 3; i++) {
        const a11y::NamedValue* row = model.find(keys[i]);
        assert(row != nullptr && row->shown && row->kind == a11y::ValueKind::toggle);
        assert(row->label == labels[i] && row->default_value == 1.0f);
        assert(model.shown()[6 + i].value == row);
    }
    assert(text.find("match_announcements.teammates = ") < text.find("damage_settle = "));
    assert(settings.values().your_stocks && settings.values().opponents_stocks &&
           settings.values().teammates);
    model.set(*model.find(keys[0]), 0.0f);
    assert(!settings.values().your_stocks && settings.values().opponents_stocks);
    model.set(*model.find(keys[1]), 0.0f);
    assert(!settings.values().opponents_stocks && settings.values().teammates);
    model.set(*model.find(keys[2]), 0.0f);
    assert(!settings.values().teammates);

    /* Time left last, a switch on by default, read live, before the settle
     * time in the file. */
    const a11y::NamedValue* time_left = model.find(a11y::MatchAnnouncementSettings::kTimeLeftKey);
    assert(time_left != nullptr && time_left->shown && time_left->kind == a11y::ValueKind::toggle);
    assert(time_left->label == "Time left" && time_left->default_value == 1.0f);
    assert(model.shown()[9].value == time_left);
    assert(text.find("match_announcements.teammates = ") <
               text.find("match_announcements.time_left = ") &&
           text.find("match_announcements.time_left = ") < text.find("damage_settle = "));
    assert(settings.values().time_left);
    model.set(*time_left, 0.0f);
    assert(!settings.values().time_left);
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
    settling_through_a_combo();
    a_rise_and_a_fall();
    a_heal_is_said_once();
    a_ko_resets_silently();
    the_baseline_at_go();
    game_drops_a_pending_figure();
    labels();
    no_you();
    labels_are_fixed_at_go();
    no_panel_no_announcement();
    hidden_hud_holds();
    the_switch();
    thresholds_crossings();
    thresholds_a_fall_then_a_crossing();
    thresholds_a_ko();
    thresholds_changes_take_effect_at_once();
    thresholds_a_teammate();
    thresholds_at_game();
    stocks_a_loss_and_last_stock();
    stocks_a_steal();
    stocks_switches();
    stocks_not_in_a_timed_match();
    the_teammates_switch();
    a_stock_line_replaces_a_waiting_damage_line();
    time_left_each_minute();
    time_left_not_a_whole_minute();
    time_left_counting_up_and_none();
    time_left_a_pause();
    time_left_the_switches();
    the_rows();
    std::cout << "match_announcer: all tests passed\n";
    return 0;
}
