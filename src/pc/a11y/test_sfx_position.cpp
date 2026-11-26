/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Positional sound effects against hand-written snapshots on Final
 * Destination's frame and a fake engine: the fighter tracks of every block,
 * slot and sub fighter, and the tracks of other sounds; the engine's pan at
 * the centre and both sides; the level within the near distance, at the far
 * distance and beyond, between them, and with a far level of zero; the pan
 * from you and across the stage. The driver: a fighter's sound placed as it
 * starts, your own centred at its own volume, other sounds and every sound
 * while off or outside a match left alone, a followed voice panned again as
 * its fighter moves and let go once it has ended, and everything forgotten
 * at a scene. A sound on no fighter's track placed when the fighter hook
 * names it just before, for that start alone and within the tick. A hit's sound placed
 * by the track its caller asked for, once and within the tick. The pan a
 * sound's own script sets, taken as an offset from a placed voice's place. Supplies its own
 * pc_log_line. */
#include "sfx_position.hpp"
#include "sfx_script_pan.hpp"
#include "test_match_snapshot.hpp"
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <map>

extern "C" void pc_log_line(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
}

namespace {

using a11y::SfxMapping;
using a11y::SfxPlace;
using a11y::SfxPositions;
using a11y::SfxSource;
using match_test::fighter;
using match_test::final_destination;

bool near(float a, float b, float tolerance = 1e-3f) {
    return std::fabs(a - b) <= tolerance;
}

void test_tracks() {
    using a11y::sfx_track_source;
    assert((sfx_track_source(0x1E) == SfxSource{0, 0}));
    assert((sfx_track_source(0x1F) == SfxSource{0, 1}));
    assert((sfx_track_source(0x20) == SfxSource{1, 0}));
    assert((sfx_track_source(0x29) == SfxSource{5, 1}));
    assert((sfx_track_source(0x2A) == SfxSource{0, 0}));
    assert((sfx_track_source(0x5A + 7) == SfxSource{3, 1}));
    assert((sfx_track_source(0x72) == SfxSource{0, 0}));
    assert((sfx_track_source(0x7D) == SfxSource{5, 1}));
    /* The stage's, the game's own and the gap in the blocks. */
    assert(!sfx_track_source(0).has_value());
    assert(!sfx_track_source(0x19).has_value());
    assert(!sfx_track_source(0x1A).has_value());
    assert(!sfx_track_source(0x1D).has_value());
    assert(!sfx_track_source(0x66).has_value());
    assert(!sfx_track_source(0x71).has_value());
    assert((sfx_track_source(0x7E) == SfxSource{0, 0}));
    assert((sfx_track_source(0x89) == SfxSource{5, 1}));
    assert(!sfx_track_source(0x8A).has_value());
}

void test_engine_pan() {
    using a11y::sfx_engine_pan;
    assert(sfx_engine_pan(0.0f) == 0x80);
    assert(sfx_engine_pan(-1.0f) == 0);
    assert(sfx_engine_pan(1.0f) == 0xFF);
    assert(sfx_engine_pan(-5.0f) == 0);
    assert(sfx_engine_pan(5.0f) == 0xFF);
    assert(sfx_engine_pan(-0.5f) == 64);
    assert(sfx_engine_pan(0.5f) > 0x80 && sfx_engine_pan(0.5f) < 0xFF);
}

void test_distance_gain() {
    using a11y::sfx_distance_gain;
    SfxMapping m;
    m.near_distance = 10.0f;
    m.far_distance = 110.0f;
    m.far_level = 0.25f;
    assert(near(sfx_distance_gain(0.0f, m), 1.0f));
    assert(near(sfx_distance_gain(10.0f, m), 1.0f));
    assert(near(sfx_distance_gain(110.0f, m), 0.25f));
    assert(near(sfx_distance_gain(500.0f, m), 0.25f));
    /* Midway in decibels: the square root of the far level. */
    assert(near(sfx_distance_gain(60.0f, m), 0.5f));
    m.far_level = 0.0f;
    assert(near(sfx_distance_gain(60.0f, m), 0.5f));
    assert(near(sfx_distance_gain(200.0f, m), 0.0f));
    m.far_level = 1.0f;
    assert(near(sfx_distance_gain(200.0f, m), 1.0f));
}

void test_place() {
    const A11yStageFrame stage = final_destination();
    SfxMapping m;
    m.side_distance = 100.0f;
    /* From you: beside you centred, at the side distance fully over. */
    assert(a11y::sfx_place(stage, 20.0f, 0.0f, 20.0f, 0.0f, m).pan == 0x80);
    assert(a11y::sfx_place(stage, 20.0f, 0.0f, 120.0f, 0.0f, m).pan == 0xFF);
    assert(a11y::sfx_place(stage, 20.0f, 0.0f, -30.0f, 0.0f, m).pan == 64);
    assert(a11y::sfx_place(stage, 20.0f, 0.0f, -300.0f, 0.0f, m).pan == 0);
    /* Straight above is centred but further, so quieter. */
    const SfxPlace above = a11y::sfx_place(stage, 0.0f, 0.0f, 0.0f, 100.0f, m);
    assert(above.pan == 0x80 && above.gain < 1.0f);
    /* On the stage: the stage's centre is centred wherever you are. */
    m.relative = false;
    assert(a11y::sfx_place(stage, 120.0f, 0.0f, 0.0f, 0.0f, m).pan == 0x80);
    assert(a11y::sfx_place(stage, 0.0f, 0.0f, 170.0f, 0.0f, m).pan == 0xFF);
}

/* You in slot 0 at the centre, the opponent in slot 1 at x. */
struct Scene {
    A11yMatchState match{};
    A11ySfxSources sources{};

    explicit Scene(float opponent_x) {
        match.running = true;
        match.stage = final_destination();
        match.you = fighter(0, 0.0f, 0.0f);
        sources.fighters[0][0] = A11ySfxFighter{true, 0.0f, 0.0f};
        move(opponent_x);
    }
    void move(float x) { sources.fighters[1][0] = A11ySfxFighter{true, x, 0.0f}; }
};

/* A fake engine: each voice's pan, the place remembered for its script,
 * and the voices that have ended. */
struct Engine {
    std::map<int, std::uint8_t> pans;
    std::map<int, std::uint8_t> remembered;
    std::map<int, bool> ended;

    SfxPositions::Engine calls() {
        return SfxPositions::Engine{
            [this](int voice, std::uint8_t pan) {
                if (ended[voice]) {
                    return false;
                }
                pans[voice] = pan;
                return true;
            },
            [this](int voice, std::uint8_t pan) { remembered[voice] = pan; },
        };
    }
};

void test_driver() {
    Engine engine;
    SfxPositions sfx(engine.calls());
    SfxMapping m;
    Scene scene(100.0f);

    /* Before any tick nothing is placed. */
    std::uint8_t volume = 200;
    std::uint8_t pan = 0x80;
    std::uint8_t script = 0x80;
    sfx.started(1, 0x20, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80 && sfx.followed() == 0);

    sfx.tick(true, scene.match, scene.sources, m);
    /* The opponent's sound: to the right, quieter than at hand. */
    sfx.started(1, 0x20, &volume, &pan, &script);
    assert(pan > 0xC0);
    assert(volume < 200 && volume > 0);
    assert(sfx.followed() == 1);
    /* Your own: centred at its own volume. */
    volume = 200;
    pan = 0x40;
    sfx.started(2, 0x2A, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);
    /* The stage's: left alone, not followed. */
    volume = 200;
    pan = 0x40;
    sfx.started(3, 0x19, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x40);
    assert(sfx.followed() == 2);
    /* No fighter in slot 4: left alone. */
    sfx.started(4, 0x1E + 8, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x40);

    /* Each placed voice's place is remembered for its script. */
    assert(engine.remembered[1] > 0xC0);
    assert(engine.remembered[2] == 0x80);
    assert(engine.remembered.count(3) == 0);

    /* The opponent crosses to your left: their voice follows, and its
     * remembered place with it. */
    scene.move(-100.0f);
    sfx.tick(true, scene.match, scene.sources, m);
    assert(engine.pans[1] < 0x40);
    assert(engine.remembered[1] < 0x40);
    assert(engine.pans[2] == 0x80);

    /* The opponent's voice ends: let go at the next tick. */
    engine.ended[1] = true;
    sfx.tick(true, scene.match, scene.sources, m);
    assert(sfx.followed() == 1);

    /* Off: forgotten, and a start is the game's own. */
    sfx.tick(false, scene.match, scene.sources, m);
    assert(sfx.followed() == 0);
    volume = 200;
    pan = 0x80;
    sfx.started(5, 0x20, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);

    /* Outside a match likewise. */
    scene.match.running = false;
    sfx.tick(true, scene.match, scene.sources, m);
    sfx.started(6, 0x20, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);

    /* A scene forgets what the last tick placed. */
    scene.match.running = true;
    sfx.tick(true, scene.match, scene.sources, m);
    sfx.forget();
    sfx.started(7, 0x20, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);
}

/* Sounds on no fighter's track, named by the fighter hook just before
 * they start. */
void test_named_starts() {
    Engine engine;
    SfxPositions sfx(engine.calls());
    SfxMapping m;
    Scene scene(-100.0f);
    sfx.tick(true, scene.match, scene.sources, m);

    /* The opponent's sound on track 0, named first: every layer to the
     * left, quieter. */
    std::uint8_t volume = 200;
    std::uint8_t pan = 0x80;
    std::uint8_t script = 0x80;
    sfx.fighter_next(SfxSource{1, 0});
    sfx.started(10, 0, &volume, &pan, &script);
    assert(pan == 0 && script == 0);
    assert(volume < 200 && volume > 0);
    assert(sfx.followed() == 1);

    /* The name is used up: the next start is no fighter's. */
    volume = 200;
    pan = 0x80;
    script = 0x80;
    sfx.started(11, 0, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80 && script == 0x80);

    /* Your own, named: centred, its volume untouched. */
    sfx.fighter_next(SfxSource{0, 0});
    sfx.started(12, 0, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80 && script == 0x80);

    /* A fighter's own track wins over a name. */
    sfx.fighter_next(SfxSource{0, 0});
    sfx.started(13, 0x20, &volume, &pan, &script);
    assert(pan == 0);

    /* A name the last tick left unused belongs to no start. */
    volume = 200;
    pan = 0x80;
    sfx.fighter_next(SfxSource{1, 0});
    sfx.tick(true, scene.match, scene.sources, m);
    sfx.started(14, 0, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);

    /* Off: never placed. */
    sfx.tick(false, scene.match, scene.sources, m);
    sfx.fighter_next(SfxSource{1, 0});
    sfx.started(15, 0, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);
}

/* A hit's sound: the track its caller asked for, dropped by the start. */
void test_track_hint() {
    Engine engine;
    SfxPositions sfx(engine.calls());
    SfxMapping m;
    Scene scene(100.0f);
    sfx.tick(true, scene.match, scene.sources, m);

    /* The opponent is hit: placed on the right as it starts. */
    std::uint8_t volume = 200;
    std::uint8_t pan = 0x80;
    std::uint8_t script = 0x80;
    sfx.track_hint(0x7E + 2);
    sfx.started(20, 0, &volume, &pan, &script);
    assert(pan > 0xC0 && volume < 200);
    assert(sfx.followed() == 1);

    /* Used once: the next start is not placed by it. */
    volume = 200;
    pan = 0x80;
    sfx.started(21, 0, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);

    /* An item's hit asks for no track. */
    sfx.track_hint(-1);
    sfx.started(22, 0, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);

    /* A hint the tick left unused is dropped. */
    sfx.track_hint(0x7E + 2);
    sfx.tick(true, scene.match, scene.sources, m);
    sfx.started(23, 0, &volume, &pan, &script);
    assert(volume == 200 && pan == 0x80);
}

/* A sound's own script's absolute pan: an offset from the voice's place,
 * for a placed voice only, held at the ends, and never another voice's
 * place in the same engine slot. */
void test_script_pan() {
    a11y::sfx_forget_pans();
    /* Not placed: the script's own. */
    assert(a11y::sfx_script_pan(5, 122) == 122);
    /* Placed fully right: the script's nudge left of centre kept. */
    a11y::sfx_remember_pan(5, 0xFF);
    assert(a11y::sfx_script_pan(5, 122) == 0xF9);
    assert(a11y::sfx_script_pan(5, 0x80) == 0xFF);
    assert(a11y::sfx_script_pan(5, 0xC0) == 0xFF);
    /* Placed left. */
    a11y::sfx_remember_pan(6, 0x10);
    assert(a11y::sfx_script_pan(6, 0x40) == 0);
    assert(a11y::sfx_script_pan(6, 0x90) == 0x20);
    /* A later voice in slot 5 (the engine's id is slot | count << 7) is not
     * the placed one. */
    assert(a11y::sfx_script_pan(5 + 0x80, 122) == 122);
    a11y::sfx_remember_pan(5 + 0x80, 0x40);
    assert(a11y::sfx_script_pan(5, 122) == 122);
    assert(a11y::sfx_script_pan(5 + 0x80, 0x80) == 0x40);
    /* No voice. */
    assert(a11y::sfx_script_pan(-1, 0x90) == 0x90);
    a11y::sfx_forget_pans();
}

void test_stage_mode_pans_your_own() {
    Engine engine;
    SfxPositions sfx(engine.calls());
    SfxMapping m;
    m.relative = false;
    Scene scene(0.0f);
    scene.match.you = fighter(0, 170.0f, 0.0f);
    scene.sources.fighters[0][0] = A11ySfxFighter{true, 170.0f, 0.0f};
    sfx.tick(true, scene.match, scene.sources, m);
    std::uint8_t volume = 200;
    std::uint8_t pan = 0x80;
    std::uint8_t script = 0x80;
    /* Your own, on the stage's right edge: to the right at its own
     * volume. */
    sfx.started(1, 0x1E, &volume, &pan, &script);
    assert(pan == 0xFF && volume == 200);
}

}  // namespace

int main() {
    test_tracks();
    test_engine_pan();
    test_distance_gain();
    test_place();
    test_driver();
    test_named_starts();
    test_track_hint();
    test_script_pan();
    test_stage_mode_pans_your_own();
    std::cout << "sfx_position: all tests passed\n";
    return 0;
}
