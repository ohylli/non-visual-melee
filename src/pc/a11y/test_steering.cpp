/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The glide against a simulated hand that moves as character select's does:
 * the game's formula, its clamping and the hand's bounds, with each stick
 * the glide asks for applied some frames after it was published. Glides run
 * between character select's targets (test_css_screen.hpp). */
#include "css_targets.hpp"
#include "steering.hpp"
#include "test_css_screen.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

namespace {

using a11y::Glide;
using a11y::GlideEnd;
using a11y::GlideFrame;
using a11y::Point;
using a11y::Stick;
using a11y::Target;
using a11y::TargetKind;
using namespace css_test;

/* Where the hand starts, over player 1's slot. */
constexpr Point kHome{-31.0f, -21.5f};
constexpr int kPeachPortrait = 4, kRoyPortrait = 24;
/* A frame of movement at a full push: the hand must end at least this far
 * inside its portrait. */
constexpr float kMargin = 1.24f;
/* The longest glide at no delay, in frames: 1.5 seconds. */
constexpr int kLongestGlide = 90;

/* The game's clamping (HSD_PadClamp, as gmmain.c sets it up), as a run
 * measured it: a stick within a full push of 80 arrives as sent, one further
 * out is scaled back onto that circle, each axis truncated. */
Stick clamped(Stick stick) {
    float x = static_cast<float>(stick.x);
    float y = static_cast<float>(stick.y);
    float length = std::sqrt(x * x + y * y);
    if (length > 80.0f) {
        return Stick{static_cast<int>(x * 80.0f / length), static_cast<int>(y * 80.0f / length)};
    }
    return stick;
}

/* Online, an axis of 2 or less travels as 0. */
Stick over_the_wire(Stick stick) {
    return Stick{std::abs(stick.x) <= 2 ? 0 : stick.x, std::abs(stick.y) <= 2 ? 0 : stick.y};
}

/* A held slider, as updateGrabbedSlider keeps the hand on it: x within 10
 * units of its lowest end, y at its knob's height. */
struct Slider {
    bool held = false;
    float origin = 0.0f;
    float y = 0.0f;
};

/* The hand after one frame, kept inside the screen as mnCharSel_CursorThink
 * keeps it, or on its slider. */
Point moved(Point hand, Stick applied, const Slider& slider) {
    Point step = a11y::cursor_step(applied);
    hand.x = std::fmin(std::fmax(hand.x + step.x, -35.0f), 26.0f);
    hand.y = std::fmin(std::fmax(hand.y + step.y, -22.0f), 25.0f);
    if (slider.held) {
        hand.x = std::fmin(std::fmax(hand.x, slider.origin), slider.origin + 10.0f);
        hand.y = slider.y;
    }
    return hand;
}

/* How the sticks the glide asks for reach the game. Once a video frame the
 * pad is published; each simulated frame then takes a sample of it and
 * applies the sample taken delay frames before. Offline the delay is 0: a
 * stick asked for at the end of one frame moves the hand in the next. */
struct Channel {
    int delay = 0;
    /* The stick published in this video frame is lost: no sample takes it. */
    int skip_at = -1;
    /* This video frame simulates two frames, both sampling what was
     * published; the first one's request is replaced before it is
     * published. */
    int repeat_at = -1;
    /* A resting stick's drift on the same controller, merged as the pad
     * merges: each axis the one pushed further. */
    Stick drift;
    /* From this simulated frame on, the player holds this stick. */
    int player_from = -1;
    Stick player;
    /* In this simulated frame A clicks the HMN/CPU button under the hand,
     * which moves the hand to this height. */
    int click_at = -1;
    float click_y = 0.0f;
    /* The hand holds a slider throughout. */
    Slider slider;
};

struct Run {
    GlideEnd end = GlideEnd::none;
    int frames = 0;
    Point hand;
};

int dominant(int a, int b) {
    return std::abs(a) > std::abs(b) ? a : b;
}

/* A glide from the hand at from to the point to, until it ends. restart_at
 * turns it to restart_to in that video frame, as a press during a glide
 * does. */
Run glide(
    Point from, Point to, const Channel& channel, int restart_at = -1, Point restart_to = Point{}) {
    Glide glide;
    Point hand = from;
    /* The samples by simulated frame. */
    std::map<int, Stick> samples;
    Stick request;
    int tick = 0;
    glide.start(to);
    for (int frame = 1; frame <= 400; frame++) {
        if (frame == restart_at) {
            glide.start(restart_to);
        }
        glide.published();
        Stick pad = frame != channel.skip_at ? over_the_wire(request) : Stick{};
        request = Stick{};
        for (int n = 0; n < (frame == channel.repeat_at ? 2 : 1); n++) {
            tick++;
            samples[tick] = pad;
            Stick sent =
                samples.count(tick - channel.delay) != 0 ? samples[tick - channel.delay] : Stick{};
            Stick own = channel.player_from >= 0 && tick >= channel.player_from ? channel.player :
                                                                                  channel.drift;
            Stick applied = clamped(Stick{dominant(sent.x, own.x), dominant(sent.y, own.y)});
            hand = moved(hand, applied, channel.slider);
            if (tick == channel.click_at) {
                hand.y = channel.click_y;
            }
            GlideFrame out = glide.frame(hand, applied);
            if (out.end != GlideEnd::none) {
                if (out.end == GlideEnd::arrived) {
                    /* Nothing is still on its way. */
                    for (int later = tick - channel.delay + 1; later <= tick; later++) {
                        assert(samples[later] == Stick{});
                    }
                }
                return Run{out.end, glide.frames(), hand};
            }
            request = out.stick;
            /* Within a full push, and nothing the wire would round to
             * rest. */
            assert(request.x * request.x + request.y * request.y <= 80 * 80);
            assert(request.x == 0 || std::abs(request.x) > 2);
            assert(request.y == 0 || std::abs(request.y) > 2);
        }
    }
    return Run{GlideEnd::none, 400, hand};
}

/* The hand has come to rest with the coin inside the portrait, a frame's
 * movement away from each edge. */
bool well_inside(Point hand, int portrait) {
    A11yCssRect rect = portrait_rect(portrait);
    float x = hand.x + a11y::kCoinOffsetX;
    float y = hand.y + a11y::kCoinOffsetY;
    return x > rect.left + kMargin && x < rect.right - kMargin && y < rect.top - kMargin &&
           y > rect.bottom + kMargin;
}

Point aim(int portrait) {
    return a11y::aim_point(vs_screen(), Target{TargetKind::portrait, portrait});
}

void the_game_formula() {
    /* The speeds the spec gives: a tilt of 20 moves 0.04 a frame, 40 moves
     * 0.28, a full push 1.24; below a squared tilt of 200 nothing moves. */
    assert(std::fabs(a11y::cursor_step(Stick{20, 0}).x - 0.04f) < 1e-4f);
    assert(std::fabs(a11y::cursor_step(Stick{0, 40}).y - 0.28f) < 1e-4f);
    assert(std::fabs(a11y::cursor_step(Stick{-80, 0}).x + 1.24f) < 1e-4f);
    assert(!a11y::moves(Stick{10, 10}));
    assert(a11y::moves(Stick{15, 0}));
}

void every_portrait_to_every_other_at_each_delay() {
    int longest = 0;
    for (int delay = 0; delay <= 10; delay++) {
        Channel channel;
        channel.delay = delay;
        for (int from = 0; from < A11Y_CSS_PORTRAITS; from++) {
            for (int to = 0; to < A11Y_CSS_PORTRAITS; to++) {
                if (to == from) {
                    continue;
                }
                Run run = glide(aim(from), aim(to), channel);
                assert(run.end == GlideEnd::arrived);
                assert(well_inside(run.hand, to));
                if (delay == 0 && run.frames > longest) {
                    longest = run.frames;
                }
            }
        }
    }
    /* Corner to corner, and from the hand's start to the far corner. */
    Run run = glide(kHome, aim(8), Channel{});
    assert(run.end == GlideEnd::arrived && well_inside(run.hand, 8));
    if (run.frames > longest) {
        longest = run.frames;
    }
    std::cout << "steering: longest glide at no delay " << longest << " frames\n";
    assert(longest < kLongestGlide);
}

void a_repeated_sample() {
    for (int delay : {0, 2, 10}) {
        for (int from = 0; from < A11Y_CSS_PORTRAITS; from++) {
            for (int to = 0; to < A11Y_CSS_PORTRAITS; to += 3) {
                if (to == from) {
                    continue;
                }
                Channel channel;
                channel.delay = delay;
                /* At full speed, slowing down, and near the end. */
                channel.repeat_at = 2 + (from * 7 + to) % 40;
                Run run = glide(aim(from), aim(to), channel);
                assert(run.end == GlideEnd::arrived);
                assert(well_inside(run.hand, to));
            }
        }
    }
}

void a_skipped_sample() {
    for (int delay : {0, 2, 10}) {
        for (int from = 0; from < A11Y_CSS_PORTRAITS; from++) {
            for (int to = 0; to < A11Y_CSS_PORTRAITS; to += 3) {
                if (to == from) {
                    continue;
                }
                Channel channel;
                channel.delay = delay;
                channel.skip_at = 1 + (from * 5 + to) % 40;
                Run run = glide(aim(from), aim(to), channel);
                assert(run.end == GlideEnd::arrived);
                assert(well_inside(run.hand, to));
            }
        }
    }
}

void a_resting_stick_does_not_stop_it() {
    Channel channel;
    channel.delay = 2;
    channel.drift = Stick{6, -9};
    Run run = glide(kHome, aim(kFoxPortrait), channel);
    assert(run.end == GlideEnd::arrived);
    assert(well_inside(run.hand, kFoxPortrait));
}

void the_players_stick_takes_over() {
    Channel channel;
    channel.delay = 3;
    channel.player_from = 20;
    channel.player = Stick{0, -80};
    Run run = glide(aim(0), aim(8), channel);
    assert(run.end == GlideEnd::abandoned);
    assert(run.frames == 20);
}

void a_new_destination_on_the_way() {
    Channel channel;
    channel.delay = 4;
    Run run = glide(aim(0), aim(8), channel, 15, aim(kMarthPortrait));
    assert(run.end == GlideEnd::arrived);
    assert(well_inside(run.hand, kMarthPortrait));
}

/* The hand has come to rest on the target: a frame's movement at a full push
 * either way still reaches it. For a portrait with no coin on it, where the
 * coin sits. */
bool well_on(const A11yCssState& state, Target target, Point hand) {
    for (Point off :
        {Point{kMargin, 0.0f}, Point{-kMargin, 0.0f}, Point{0.0f, kMargin}, Point{0.0f, -kMargin}})
    {
        if (!(a11y::target_at(state, hand.x + off.x, hand.y + off.y) == target)) {
            return false;
        }
    }
    return true;
}

/* A at the hand picks up the coin of the slot (mnCharSel_CursorThink), even
 * a frame's movement away: within 3 units of the hand plus (3.8, -2.6), the
 * hand in the portrait band. */
bool picks_up(const A11yCssState& state, Point hand, int slot) {
    float dx = hand.x + 3.8f - state.slots[slot].coin.x;
    float dy = hand.y - 2.6f - state.slots[slot].coin.y;
    return std::sqrt(dx * dx + dy * dy) < 3.0f - kMargin && hand.y > 0.2f && hand.y < 22.0f;
}

/* A team match with the handicap rule on: every kind of target in the rows.
 * Player 1 is human with a free hand, player 2 a CPU; their handicap knobs
 * and player 2's CPU level knob, moved down to its second place, stand apart
 * from each other and from player 1's name box. */
A11yCssState every_kind_of_target() {
    A11yCssState state = vs_screen();
    state.teams = true;
    state.handicap_sliders = true;
    state.slots[0].handicap_knob = A11yCssKnob{true, -30.9f, -12.62f, -30.9f};
    state.slots[1].handicap_knob = A11yCssKnob{true, -15.5f, -12.62f, -15.5f};
    state.slots[1].cpu_level_knob = A11yCssKnob{true, -15.5f, -18.12f, -15.5f};
    /* Player 1's coin at rest too, so no coin jumps into the hand. */
    state.slots[0].portrait = kFoxPortrait;
    state.slots[0].over_portrait = kFoxPortrait;
    state.slots[0].character = kFox;
    state.slots[0].coin = coin_at_rest(kFoxPortrait);
    return state;
}

/* Glides from every target in the screen's rows to every other, at delays 0
 * to 10, but portrait to portrait, which is glided between above; each ends
 * well on its target, or where A picks up the coin resting there. The
 * longest at no delay. */
int glides_among_the_rows(const A11yCssState& state) {
    std::vector<Target> targets;
    for (const std::vector<Target>& row : a11y::target_rows(state)) {
        targets.insert(targets.end(), row.begin(), row.end());
    }
    int longest = 0;
    for (int delay = 0; delay <= 10; delay++) {
        Channel channel;
        channel.delay = delay;
        for (Target from : targets) {
            for (Target to : targets) {
                bool portraits =
                    from.kind == TargetKind::portrait && to.kind == TargetKind::portrait;
                if (to == from || portraits) {
                    continue;
                }
                Run run = glide(a11y::aim_point(state, from), a11y::aim_point(state, to), channel);
                assert(run.end == GlideEnd::arrived);
                int coin =
                    to.kind == TargetKind::portrait ? a11y::pickable_coin(state, to.index) : -1;
                if (coin >= 0) {
                    assert(picks_up(state, run.hand, coin));
                } else if (to.kind == TargetKind::portrait) {
                    assert(well_inside(run.hand, to.index));
                } else {
                    assert(well_on(state, to, run.hand));
                }
                if (delay == 0 && run.frames > longest) {
                    longest = run.frames;
                }
            }
        }
    }
    return longest;
}

void onto_every_kind_of_target_at_each_delay() {
    /* Yoshi and Fox, with coins on them, are aimed at to pick those up; A
     * there would. */
    int longest = glides_among_the_rows(every_kind_of_target());
    std::cout << "steering: longest glide to any target at no delay " << longest << " frames\n";
    assert(longest < kLongestGlide);
}

void onto_every_single_player_target_at_each_delay() {
    /* Classic's arrows and Training's CPU coin, player 1's coin at rest on
     * Fox so no coin jumps into the hand. */
    for (A11yCssState state : {classic_screen(), training_screen()}) {
        state.slots[0].portrait = kFoxPortrait;
        state.slots[0].over_portrait = kFoxPortrait;
        state.slots[0].character = kFox;
        state.slots[0].coin = coin_at_rest(kFoxPortrait);
        assert(glides_among_the_rows(state) < kLongestGlide);
    }
}

void onto_a_coin_to_pick_up_at_each_delay() {
    /* Player 2's coin where the game put it on Yoshi, drifted to the corners
     * of Yoshi's portrait, and player 1's own on Fox. */
    A11yCssState state = every_kind_of_target();
    A11yCssRect yoshi = portrait_rect(kYoshiPortrait);
    const Point corners[] = {Point{yoshi.left + kCoinRestRight, yoshi.top - kCoinRestDown},
        Point{yoshi.left, yoshi.top}, Point{yoshi.right, yoshi.top},
        Point{yoshi.left, yoshi.bottom}, Point{yoshi.right, yoshi.bottom}};
    for (int delay = 0; delay <= 10; delay++) {
        Channel channel;
        channel.delay = delay;
        for (Point coin : corners) {
            state.slots[1].coin.x = coin.x;
            state.slots[1].coin.y = coin.y;
            for (Point from : {kHome, Point{20.0f, 24.0f}, aim(kRoyPortrait)}) {
                Run run = glide(from,
                    a11y::aim_point(state, Target{TargetKind::portrait, kYoshiPortrait}), channel);
                assert(run.end == GlideEnd::arrived);
                assert(picks_up(state, run.hand, 1));
            }
        }
        Run run = glide(
            kHome, a11y::aim_point(state, Target{TargetKind::portrait, kFoxPortrait}), channel);
        assert(run.end == GlideEnd::arrived);
        assert(picks_up(state, run.hand, 0));
    }
}

void a_held_slider_from_each_value_to_each_other_at_each_delay() {
    A11yCssState state = vs_screen();
    state.hand.slider = A11Y_CSS_CPU_LEVEL;
    state.hand.slider_slot = 1;
    const A11yCssKnob& knob = state.slots[1].cpu_level_knob;
    for (int delay = 0; delay <= 10; delay++) {
        Channel channel;
        channel.delay = delay;
        channel.slider = Slider{true, knob.origin_x, knob.y};
        for (int from = a11y::kLowestLevel; from <= a11y::kHighestLevel; from++) {
            for (int to = a11y::kLowestLevel; to <= a11y::kHighestLevel; to++) {
                if (to == from) {
                    continue;
                }
                Point start = a11y::aim_point(state, Target{TargetKind::cpu_level, 1, from});
                Point end = a11y::aim_point(state, Target{TargetKind::cpu_level, 1, to});
                Run run = glide(start, end, channel);
                assert(run.end == GlideEnd::arrived);
                /* On the value, and a little movement either way still is. */
                assert(slider_value(knob.origin_x, run.hand.x - 0.25f) == to);
                assert(slider_value(knob.origin_x, run.hand.x + 0.25f) == to);
            }
        }
    }
}

void a_click_on_the_hmn_cpu_button_on_the_way() {
    /* A pressed as the hand reaches player 2's button moves it to the
     * button's middle height, which is where the glide aims. */
    A11yCssState state = vs_screen();
    Target button{TargetKind::slot_button, 1};
    for (int delay : {0, 3, 10}) {
        for (int click_at = 5; click_at < 40; click_at += 5) {
            Channel channel;
            channel.delay = delay;
            channel.click_at = click_at;
            channel.click_y = -2.2f;
            Run run = glide(aim(kPeachPortrait), a11y::aim_point(state, button), channel);
            assert(run.end == GlideEnd::arrived);
            assert(well_on(state, button, run.hand));
        }
    }
}

void out_of_reach_fails() {
    /* Beyond the right edge the hand is kept inside. */
    Run run = glide(aim(0), Point{40.0f, 10.0f}, Channel{});
    assert(run.end == GlideEnd::failed);
}

}  // namespace

int main() {
    the_game_formula();
    every_portrait_to_every_other_at_each_delay();
    a_repeated_sample();
    a_skipped_sample();
    a_resting_stick_does_not_stop_it();
    the_players_stick_takes_over();
    a_new_destination_on_the_way();
    onto_every_kind_of_target_at_each_delay();
    onto_every_single_player_target_at_each_delay();
    onto_a_coin_to_pick_up_at_each_delay();
    a_held_slider_from_each_value_to_each_other_at_each_delay();
    a_click_on_the_hmn_cpu_button_on_the_way();
    out_of_reach_fails();
    std::cout << "steering: all tests passed\n";
    return 0;
}
