/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Stage sounds against a fake engine: the stages' banks and the banks
 * around them; a stage's sound muted as it starts and held at 0 each tick,
 * a fighter's or the game's own left alone, nothing muted with the switch
 * on, each muted voice given its volume back when it is switched on again,
 * and a voice that has ended let go. Supplies its own pc_log_line. */
#include "stage_sounds.hpp"
#include <cassert>
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

using a11y::StageSounds;

constexpr int kCorneria = 35;
constexpr int kMain = 0;
constexpr int kGanon = 30;

void test_banks() {
    assert(!a11y::is_stage_bank(kMain));
    assert(!a11y::is_stage_bank(kGanon));
    assert(!a11y::is_stage_bank(33)); /* kirbytm.ssm */
    assert(a11y::is_stage_bank(34));
    assert(a11y::is_stage_bank(kCorneria));
    assert(a11y::is_stage_bank(48));
    assert(!a11y::is_stage_bank(49)); /* 1padv.ssm */
    assert(!a11y::is_stage_bank(55)); /* none */
    assert(a11y::stage_bank_name(kCorneria) == "corneria");
    assert(a11y::stage_bank_name(48) == "pstadium");
    assert(a11y::stage_bank_name(kMain).empty());
}

struct Engine {
    std::map<int, std::uint8_t> volumes;
    std::map<int, bool> ended;

    StageSounds::SetVolume set_volume() {
        return [this](int voice, std::uint8_t volume) {
            if (ended[voice]) {
                return false;
            }
            volumes[voice] = volume;
            return true;
        };
    }
};

void test_mute() {
    Engine engine;
    StageSounds stage(engine.set_volume());

    /* On: nothing muted. */
    std::uint8_t volume = 200;
    assert(!stage.started(false, 1, kCorneria, &volume));
    assert(volume == 200 && stage.muted() == 0);

    /* Off: a stage's sound starts at 0; a fighter's and the game's own
     * play on. */
    assert(stage.started(true, 2, kCorneria, &volume));
    assert(volume == 0 && stage.muted() == 1);
    volume = 200;
    assert(!stage.started(true, 3, kGanon, &volume));
    assert(!stage.started(true, 4, kMain, &volume));
    assert(volume == 200);

    /* The stage fades it up; the tick holds it at 0. */
    engine.volumes[2] = 180;
    stage.tick(true);
    assert(engine.volumes[2] == 0);

    /* Ended: let go. */
    volume = 150;
    stage.started(true, 5, kCorneria, &volume);
    engine.ended[2] = true;
    stage.tick(true);
    assert(stage.muted() == 1);

    /* Switched on: the one still playing gets its own volume back. */
    stage.tick(false);
    assert(engine.volumes[5] == 150);
    assert(stage.muted() == 0);
}

}  // namespace

int main() {
    test_banks();
    test_mute();
    std::cout << "stage_sounds: all tests passed\n";
    return 0;
}
