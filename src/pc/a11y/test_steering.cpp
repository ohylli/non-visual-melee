/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The glide against a simulated hand that moves as character select's does:
 * the game's formula, its clamping and the hand's bounds, with each stick
 * the glide asks for applied some frames after it was published. Glides run
 * between character select's portraits (test_css_screen.hpp). */
#include "css_targets.hpp"
#include "steering.hpp"
#include "test_css_screen.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>

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

/* The hand after one frame, kept inside the screen as mnCharSel_CursorThink
 * keeps it. */
Point moved(Point hand, Stick applied) {
    Point step = a11y::cursor_step(applied);
    hand.x = std::fmin(std::fmax(hand.x + step.x, -35.0f), 26.0f);
    hand.y = std::fmin(std::fmax(hand.y + step.y, -22.0f), 25.0f);
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
            hand = moved(hand, applied);
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
    out_of_reach_fails();
    std::cout << "steering: all tests passed\n";
    return 0;
}
