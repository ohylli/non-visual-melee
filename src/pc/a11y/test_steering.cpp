/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The glide against simulated cursors that move as the game's do: each
 * cursor's movement rule, the pad's clamping and the cursor's bounds, with
 * each stick the glide asks for applied some frames after it was published.
 * The cases every glide must survive run for both cursors: character
 * select's hand between its portraits (test_css_screen.hpp), and stage
 * select's cursor between its tiles (test_sss_screen.hpp). The hand's own
 * targets (buttons, knobs, coins, a held slider) follow. */
#include "css_targets.hpp"
#include "steering.hpp"
#include "test_css_screen.hpp"
#include "test_sss_screen.hpp"
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
/* A frame of the hand's movement at a full push: the hand must end at least
 * this far inside a target. */
constexpr float kMargin = 1.24f;
/* The longest glide at no delay, in frames: 1.5 seconds. */
constexpr int kLongestGlide = 90;

/* A rectangle a cursor at rest must be inside. */
struct Area {
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
};

/* A point glides go to, and where the cursor must come to rest for it. */
struct Place {
    Point aim;
    Area area;
};

/* A cursor as a test moves it: the game's rule, the bounds the game keeps it
 * in, where it starts, and the places glides go between. */
struct Cursor {
    const char* name;
    const a11y::MovementRule* rule;
    Area bounds;
    Point home;
    std::vector<Place> places;
};

Point aim(int portrait) {
    return a11y::aim_point(vs_screen(), Target{TargetKind::portrait, portrait});
}

/* Character select's hand over the portraits, within mnCharSel_CursorThink's
 * bounds; a portrait is tested where the hand would carry a coin. */
Cursor hand_cursor() {
    Cursor cursor{"hand", &a11y::kHandMovement, Area{-35.0f, 26.0f, -22.0f, 25.0f}, kHome, {}};
    for (int i = 0; i < A11Y_CSS_PORTRAITS; i++) {
        A11yCssRect rect = portrait_rect(i);
        cursor.places.push_back(
            Place{aim(i), Area{rect.left - a11y::kCoinOffsetX, rect.right - a11y::kCoinOffsetX,
                              rect.bottom - a11y::kCoinOffsetY, rect.top - a11y::kCoinOffsetY}});
    }
    return cursor;
}

/* Stage select's cursor over the tiles, aimed at their centres, within
 * fn_8025A310's bounds; it starts below every tile, about here. */
Cursor stage_cursor() {
    Cursor cursor{"stage cursor", &a11y::kStageCursorMovement, Area{-27.0f, 27.0f, -19.0f, 19.0f},
        Point{0.0f, -15.0f}, {}};
    for (const sss_test::TileRow& tile : sss_test::kTiles) {
        cursor.places.push_back(Place{
            Point{tile.x, tile.y}, Area{tile.x - tile.half_width, tile.x + tile.half_width,
                                       tile.y - tile.half_height, tile.y + tile.half_height}});
    }
    return cursor;
}

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

/* The cursor after one frame, kept inside its bounds as the game keeps it,
 * or on its slider. */
Point moved(const Cursor& cursor, Point at, Stick applied, const Slider& slider) {
    Point step = cursor.rule->step(applied);
    at.x = std::fmin(std::fmax(at.x + step.x, cursor.bounds.left), cursor.bounds.right);
    at.y = std::fmin(std::fmax(at.y + step.y, cursor.bounds.bottom), cursor.bounds.top);
    if (slider.held) {
        at.x = std::fmin(std::fmax(at.x, slider.origin), slider.origin + 10.0f);
        at.y = slider.y;
    }
    return at;
}

/* How the sticks the glide asks for reach the game. Once a video frame the
 * pad is published; each simulated frame then takes a sample of it and
 * applies the sample taken delay frames before. Offline the delay is 0: a
 * stick asked for at the end of one frame moves the cursor in the next. */
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
    Point at;
};

int dominant(int a, int b) {
    return std::abs(a) > std::abs(b) ? a : b;
}

/* A glide of the cursor from the point from to the point to, until it ends.
 * restart_at turns it to restart_to in that video frame, as a press during
 * a glide does. */
Run glide(const Cursor& cursor, Point from, Point to, const Channel& channel, int restart_at = -1,
    Point restart_to = Point{}) {
    Glide glide(*cursor.rule);
    Point at = from;
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
            at = moved(cursor, at, applied, channel.slider);
            if (tick == channel.click_at) {
                at.y = channel.click_y;
            }
            GlideFrame out = glide.frame(at, applied);
            if (out.end != GlideEnd::none) {
                if (out.end == GlideEnd::arrived) {
                    /* Nothing is still on its way. */
                    for (int later = tick - channel.delay + 1; later <= tick; later++) {
                        assert(samples[later] == Stick{});
                    }
                }
                return Run{out.end, glide.frames(), at};
            }
            request = out.stick;
            /* Within a full push, and nothing the wire would round to
             * rest. */
            assert(request.x * request.x + request.y * request.y <= 80 * 80);
            assert(request.x == 0 || std::abs(request.x) > 2);
            assert(request.y == 0 || std::abs(request.y) > 2);
        }
    }
    return Run{GlideEnd::none, 400, at};
}

/* The cursor has come to rest inside the place, a frame's movement at a
 * full push away from each edge. */
bool well_inside(const Cursor& cursor, Point at, const Place& place) {
    float margin = cursor.rule->full_step;
    return at.x > place.area.left + margin && at.x < place.area.right - margin &&
           at.y < place.area.top - margin && at.y > place.area.bottom + margin;
}

/* The hand has come to rest with the coin inside the portrait. */
bool well_inside(Point hand, int portrait) {
    Cursor cursor = hand_cursor();
    return well_inside(cursor, hand, cursor.places[portrait]);
}

void the_game_formulas() {
    /* The hand's speeds the character select spec gives: a tilt of 20 moves
     * 0.04 a frame, 40 moves 0.28, a full push 1.24; below a squared tilt of
     * 200 nothing moves. */
    const a11y::MovementRule& hand = a11y::kHandMovement;
    assert(std::fabs(hand.step(Stick{20, 0}).x - 0.04f) < 1e-4f);
    assert(std::fabs(hand.step(Stick{0, 40}).y - 0.28f) < 1e-4f);
    assert(std::fabs(hand.step(Stick{-80, 0}).x + 1.24f) < 1e-4f);
    assert(std::fabs(hand.full_step - kMargin) < 1e-4f);
    assert(!hand.moves(Stick{10, 10}));
    assert(hand.moves(Stick{15, 0}));
    /* The stage cursor's: per axis, 0.03 for each step beyond 30, so a full
     * push moves 1.5; nothing within 30 on both axes. */
    const a11y::MovementRule& stage = a11y::kStageCursorMovement;
    assert(std::fabs(stage.step(Stick{40, 0}).x - 0.3f) < 1e-4f);
    assert(std::fabs(stage.step(Stick{-80, 0}).x + 1.5f) < 1e-4f);
    assert(std::fabs(stage.step(Stick{31, -30}).x - 0.03f) < 1e-4f);
    assert(stage.step(Stick{31, -30}).y == 0.0f);
    Point diagonal = stage.step(Stick{50, -60});
    assert(std::fabs(diagonal.x - 0.6f) < 1e-4f && std::fabs(diagonal.y + 0.9f) < 1e-4f);
    assert(std::fabs(stage.full_step - 1.5f) < 1e-4f);
    assert(!stage.moves(Stick{30, -30}));
    assert(!stage.moves(Stick{25, 25}));
    assert(stage.moves(Stick{0, -31}));
}

/* The place furthest from the first, for the long glides below. */
const Place& far_place(const Cursor& cursor) {
    const Place* far = &cursor.places.front();
    Point from = cursor.places.front().aim;
    for (const Place& place : cursor.places) {
        if (std::hypot(place.aim.x - from.x, place.aim.y - from.y) >
            std::hypot(far->aim.x - from.x, far->aim.y - from.y))
        {
            far = &place;
        }
    }
    return *far;
}

void every_place_to_every_other_at_each_delay(const Cursor& cursor) {
    int longest = 0;
    int count = static_cast<int>(cursor.places.size());
    for (int delay = 0; delay <= 10; delay++) {
        Channel channel;
        channel.delay = delay;
        for (int from = 0; from < count; from++) {
            for (int to = 0; to < count; to++) {
                if (to == from) {
                    continue;
                }
                Run run = glide(cursor, cursor.places[from].aim, cursor.places[to].aim, channel);
                assert(run.end == GlideEnd::arrived);
                assert(well_inside(cursor, run.at, cursor.places[to]));
                if (delay == 0 && run.frames > longest) {
                    longest = run.frames;
                }
            }
        }
    }
    /* From the start and from the corners of the bounds, to every place. */
    const Area& bounds = cursor.bounds;
    for (Point from : {cursor.home, Point{bounds.left, bounds.top}, Point{bounds.right, bounds.top},
             Point{bounds.left, bounds.bottom}, Point{bounds.right, bounds.bottom}})
    {
        for (const Place& to : cursor.places) {
            Run run = glide(cursor, from, to.aim, Channel{});
            assert(run.end == GlideEnd::arrived && well_inside(cursor, run.at, to));
            if (run.frames > longest) {
                longest = run.frames;
            }
        }
    }
    std::cout << "steering: longest " << cursor.name << " glide at no delay " << longest
              << " frames\n";
    assert(longest < kLongestGlide);
}

void a_repeated_sample(const Cursor& cursor) {
    int count = static_cast<int>(cursor.places.size());
    for (int delay : {0, 2, 10}) {
        for (int from = 0; from < count; from++) {
            for (int to = 0; to < count; to += 3) {
                if (to == from) {
                    continue;
                }
                Channel channel;
                channel.delay = delay;
                /* At full speed, slowing down, and near the end. */
                channel.repeat_at = 2 + (from * 7 + to) % 40;
                Run run = glide(cursor, cursor.places[from].aim, cursor.places[to].aim, channel);
                assert(run.end == GlideEnd::arrived);
                assert(well_inside(cursor, run.at, cursor.places[to]));
            }
        }
    }
}

void a_skipped_sample(const Cursor& cursor) {
    int count = static_cast<int>(cursor.places.size());
    for (int delay : {0, 2, 10}) {
        for (int from = 0; from < count; from++) {
            for (int to = 0; to < count; to += 3) {
                if (to == from) {
                    continue;
                }
                Channel channel;
                channel.delay = delay;
                channel.skip_at = 1 + (from * 5 + to) % 40;
                Run run = glide(cursor, cursor.places[from].aim, cursor.places[to].aim, channel);
                assert(run.end == GlideEnd::arrived);
                assert(well_inside(cursor, run.at, cursor.places[to]));
            }
        }
    }
}

void a_resting_stick_does_not_stop_it(const Cursor& cursor) {
    for (const Place& to : cursor.places) {
        Channel channel;
        channel.delay = 2;
        channel.drift = Stick{6, -9};
        Run run = glide(cursor, cursor.home, to.aim, channel);
        assert(run.end == GlideEnd::arrived);
        assert(well_inside(cursor, run.at, to));
    }
}

void the_players_stick_takes_over(const Cursor& cursor) {
    Channel channel;
    channel.delay = 3;
    channel.player_from = 20;
    channel.player = Stick{0, -80};
    Run run = glide(cursor, cursor.places.front().aim, far_place(cursor).aim, channel);
    assert(run.end == GlideEnd::abandoned);
    assert(run.frames == 20);
}

void a_new_destination_on_the_way(const Cursor& cursor) {
    const Place& middle = cursor.places[cursor.places.size() / 2];
    for (int delay : {0, 4, 10}) {
        Channel channel;
        channel.delay = delay;
        Run run = glide(
            cursor, cursor.places.front().aim, far_place(cursor).aim, channel, 15, middle.aim);
        assert(run.end == GlideEnd::arrived);
        assert(well_inside(cursor, run.at, middle));
    }
}

void out_of_reach_fails(const Cursor& cursor) {
    /* Beyond the right edge the cursor is kept inside. */
    Run run = glide(
        cursor, cursor.places.front().aim, Point{cursor.bounds.right + 14.0f, 10.0f}, Channel{});
    assert(run.end == GlideEnd::failed);
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
    Cursor hand = hand_cursor();
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
                Run run =
                    glide(hand, a11y::aim_point(state, from), a11y::aim_point(state, to), channel);
                assert(run.end == GlideEnd::arrived);
                int coin =
                    to.kind == TargetKind::portrait ? a11y::pickable_coin(state, to.index) : -1;
                if (coin >= 0) {
                    assert(picks_up(state, run.at, coin));
                } else if (to.kind == TargetKind::portrait) {
                    assert(well_inside(run.at, to.index));
                } else {
                    assert(well_on(state, to, run.at));
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
    Cursor hand = hand_cursor();
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
                Run run = glide(hand, from,
                    a11y::aim_point(state, Target{TargetKind::portrait, kYoshiPortrait}), channel);
                assert(run.end == GlideEnd::arrived);
                assert(picks_up(state, run.at, 1));
            }
        }
        Run run = glide(hand, kHome,
            a11y::aim_point(state, Target{TargetKind::portrait, kFoxPortrait}), channel);
        assert(run.end == GlideEnd::arrived);
        assert(picks_up(state, run.at, 0));
    }
}

void a_held_slider_from_each_value_to_each_other_at_each_delay() {
    Cursor hand = hand_cursor();
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
                Run run = glide(hand, start, end, channel);
                assert(run.end == GlideEnd::arrived);
                /* On the value, and a little movement either way still is. */
                assert(slider_value(knob.origin_x, run.at.x - 0.25f) == to);
                assert(slider_value(knob.origin_x, run.at.x + 0.25f) == to);
            }
        }
    }
}

void a_click_on_the_hmn_cpu_button_on_the_way() {
    /* A pressed as the hand reaches player 2's button moves it to the
     * button's middle height, which is where the glide aims. */
    Cursor hand = hand_cursor();
    A11yCssState state = vs_screen();
    Target button{TargetKind::slot_button, 1};
    for (int delay : {0, 3, 10}) {
        for (int click_at = 5; click_at < 40; click_at += 5) {
            Channel channel;
            channel.delay = delay;
            channel.click_at = click_at;
            channel.click_y = -2.2f;
            Run run = glide(hand, aim(kPeachPortrait), a11y::aim_point(state, button), channel);
            assert(run.end == GlideEnd::arrived);
            assert(well_on(state, button, run.at));
        }
    }
}

}  // namespace

int main() {
    the_game_formulas();
    for (const Cursor& cursor : {hand_cursor(), stage_cursor()}) {
        every_place_to_every_other_at_each_delay(cursor);
        a_repeated_sample(cursor);
        a_skipped_sample(cursor);
        a_resting_stick_does_not_stop_it(cursor);
        the_players_stick_takes_over(cursor);
        a_new_destination_on_the_way(cursor);
        out_of_reach_fails(cursor);
    }
    onto_every_kind_of_target_at_each_delay();
    onto_every_single_player_target_at_each_delay();
    onto_a_coin_to_pick_up_at_each_delay();
    a_held_slider_from_each_value_to_each_other_at_each_delay();
    a_click_on_the_hmn_cpu_button_on_the_way();
    std::cout << "steering: all tests passed\n";
    return 0;
}
