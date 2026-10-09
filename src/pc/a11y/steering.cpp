/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "steering.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace a11y {
namespace {

/* Character select's hand (getStickDelta in src/melee/mn/mncharsel.c). */
constexpr float kHandThreshold = 200.0f;
constexpr float kHandSpeed = 0.0002f;
/* A full push, and the game's clamping (HSD_PadClamp, set up in gmmain.c):
 * a stick further out than this is scaled back to it, anything within it
 * arrives as sent. */
constexpr int kFullPush = 80;
constexpr float kHandFullStep =
    kHandSpeed * (static_cast<float>(kFullPush * kFullPush) - kHandThreshold);
/* Stage select's cursor (src/melee/mn/mnstagesel.c): the speed beyond the
 * dead zone. */
constexpr float kStageSpeed = 0.03f;
constexpr float kStageFullStep = kStageSpeed * static_cast<float>(kFullPush - kStageDeadZone);
/* Online, a stick axis of 2 or less travels as 0 (at_rest in
 * src/pc/net_wire.c), so the glide never asks for one. */
constexpr int kWireRest = 2;
/* Two axis values this near rest count as the same: a resting stick's
 * drift, merged into the glide's by the pad merge, is no sign of the player
 * steering. Below this on both axes a stick barely moves the hand, and the
 * stage cursor not at all. */
constexpr int kRestSlack = 14;
/* The pad's clamp scales a stick beyond a full push back onto its circle, so
 * a full push of the glide's with a drift merged into its other axis
 * arrives this much shorter on an axis, at most. */
constexpr int kClampSlack = 2;

/* Nearer the destination than this, the glide asks for nothing more. Every
 * target is several units wide, and both cursors get nearer: the smallest
 * tilt that moves the hand moves it 0.005, and the stage cursor moves in
 * steps of 0.03 per axis, so it ends within 0.015 on each. */
constexpr float kAimTolerance = 0.05f;
/* A sent stick not applied this many frames after the measured delay was
 * lost: a video frame that simulated nothing, or two that simulated one. */
constexpr int kLateSlack = 2;
/* A glide longer than this has failed: the longest takes under 100 frames
 * at the longest input delay (test_steering.cpp). */
constexpr int kGiveUpFrames = 180;
/* How long a sent stick is still recognised as the glide's own. */
constexpr int kSentHistory = 60;

bool same_axis(int a, int b, int slack) {
    return std::abs(a - b) <= slack || (std::abs(a) <= kRestSlack && std::abs(b) <= kRestSlack);
}

bool at_full_push(Stick stick) {
    constexpr int kShortest = kFullPush - kClampSlack;
    return stick.x * stick.x + stick.y * stick.y >= kShortest * kShortest;
}

bool same(Stick a, Stick b) {
    int slack = at_full_push(a) && at_full_push(b) ? kClampSlack : 0;
    return same_axis(a.x, b.x, slack) && same_axis(a.y, b.y, slack);
}

int toward_zero(int value) {
    return value > 0 ? value - 1 : value + 1;
}

Point hand_step(Stick stick) {
    /* The game's own arithmetic, in floats, so a simulated hand lands where
     * the game's does. */
    float x = static_cast<float>(stick.x);
    float y = static_cast<float>(stick.y);
    float tilt_squared = x * x + y * y;
    if (tilt_squared < kHandThreshold) {
        return Point{};
    }
    float speed = tilt_squared - kHandThreshold;
    float angle = std::atan2(x, y);
    return Point{kHandSpeed * (speed * std::sin(angle)), kHandSpeed * (speed * std::cos(angle))};
}

bool hand_moves(Stick stick) {
    return static_cast<float>(stick.x * stick.x + stick.y * stick.y) > kHandThreshold;
}

Stick hand_stick_for(Point distance) {
    /* The tilt that moves the hand the whole distance, along it, at most a
     * full push. */
    float length = std::hypot(distance.x, distance.y);
    float step = std::min(length, kHandFullStep);
    float tilt = std::sqrt(step / kHandSpeed + kHandThreshold);
    return Stick{static_cast<int>(std::lround(tilt * distance.x / length)),
        static_cast<int>(std::lround(tilt * distance.y / length))};
}

/* One axis of the stage cursor's step, in the game's floats. */
float stage_axis_step(int value) {
    if (value > kStageDeadZone) {
        return kStageSpeed * static_cast<float>(value - kStageDeadZone);
    }
    if (value < -kStageDeadZone) {
        return kStageSpeed * static_cast<float>(value + kStageDeadZone);
    }
    return 0.0f;
}

Point stage_step(Stick stick) {
    return Point{stage_axis_step(stick.x), stage_axis_step(stick.y)};
}

bool stage_moves(Stick stick) {
    return std::abs(stick.x) > kStageDeadZone || std::abs(stick.y) > kStageDeadZone;
}

/* The axis value that covers distance, at most a full push; 0 for a distance
 * the smallest step would overshoot by more than it leaves. */
int stage_axis_for(float distance) {
    float step = std::min(std::fabs(distance), kStageFullStep);
    int beyond = static_cast<int>(std::lround(step / kStageSpeed));
    if (beyond == 0) {
        return 0;
    }
    return distance > 0.0f ? kStageDeadZone + beyond : -(kStageDeadZone + beyond);
}

Stick stage_stick_for(Point distance) {
    /* Each axis on its own: a diagonal beyond a full push is taken back by
     * the glide, the axis further out first, so both axes move about
     * equally until the nearer is there. */
    return Stick{stage_axis_for(distance.x), stage_axis_for(distance.y)};
}

}  // namespace

const MovementRule kHandMovement{hand_step, hand_moves, hand_stick_for, kHandFullStep};
const MovementRule kStageCursorMovement{stage_step, stage_moves, stage_stick_for, kStageFullStep};

void Glide::start(Point destination) {
    m_destination = destination;
    m_active = true;
    m_started = m_now;
}

GlideFrame Glide::frame(Point cursor, Stick applied) {
    m_now++;
    m_samples_of_pad++;
    if (m_on_pad && m_samples_of_pad > 1) {
        /* A second simulated frame in one video frame sampled the pad
         * again: its stick moves the cursor once more, a frame later. */
        Sent again = m_pad;
        again.frame += m_samples_of_pad - 1;
        m_in_flight.push_back(again);
        m_sent.push_back(again);
    }
    /* A request still waiting was never published. This frame asks
     * afresh. */
    m_request_waiting = false;
    bool own = account(applied);
    while (!m_in_flight.empty() && m_now - m_in_flight.front().frame > m_delay + kLateSlack) {
        m_in_flight.pop_front();
    }
    while (!m_sent.empty() && m_now - m_sent.front().frame > kSentHistory) {
        m_sent.pop_front();
    }
    if (!m_active) {
        return GlideFrame{};
    }
    if (!own) {
        m_active = false;
        return GlideFrame{Stick{}, GlideEnd::abandoned};
    }
    if (frames() > kGiveUpFrames) {
        m_active = false;
        return GlideFrame{Stick{}, GlideEnd::failed};
    }
    /* Where the cursor comes to rest if nothing more is asked for: the
     * sticks on their way still move it. */
    Point rest = cursor;
    for (const Sent& sent : m_in_flight) {
        rest.x += sent.step.x;
        rest.y += sent.step.y;
    }
    Point left{m_destination.x - rest.x, m_destination.y - rest.y};
    if (std::hypot(left.x, left.y) < kAimTolerance) {
        if (m_in_flight.empty()) {
            m_active = false;
            return GlideFrame{Stick{}, GlideEnd::arrived};
        }
        return GlideFrame{};
    }
    m_request = stick_for(left);
    m_request_waiting = true;
    return GlideFrame{m_request, GlideEnd::none};
}

void Glide::published() {
    m_samples_of_pad = 0;
    m_on_pad = m_request_waiting;
    if (!m_request_waiting) {
        return;
    }
    m_request_waiting = false;
    m_pad = Sent{m_request, m_now, m_rule->step(m_request)};
    m_in_flight.push_back(m_pad);
    m_sent.push_back(m_pad);
}

bool Glide::account(Stick applied) {
    if (!m_rule->moves(applied)) {
        m_last_applied = applied;
        return true;
    }
    bool repeated = same(applied, m_last_applied);
    m_last_applied = applied;
    /* Sticks arrive in the order they were sent; any sent before the one
     * that arrived were lost on the way. The same stick again before its
     * time is the last sample used twice: taking it for the next would
     * count a push as done that is still to come. */
    for (auto it = m_in_flight.begin(); it != m_in_flight.end(); ++it) {
        if (same(it->stick, applied)) {
            if (repeated && m_now - it->frame < m_delay) {
                break;
            }
            m_delay = m_now - it->frame;
            m_in_flight.erase(m_in_flight.begin(), it + 1);
            return true;
        }
    }
    /* A sample the game used twice, or one that came after it was given up
     * for lost. */
    return std::any_of(
        m_sent.begin(), m_sent.end(), [&](const Sent& sent) { return same(sent.stick, applied); });
}

Stick Glide::stick_for(Point distance) const {
    /* The stick that moves the cursor the whole distance in one frame, at
     * most a full push; each frame asks afresh, so rounding is corrected as
     * the cursor goes. */
    Stick stick = m_rule->stick_for(distance);
    if (std::abs(stick.x) <= kWireRest) {
        stick.x = 0;
    }
    if (std::abs(stick.y) <= kWireRest) {
        stick.y = 0;
    }
    while (stick.x * stick.x + stick.y * stick.y > kFullPush * kFullPush) {
        if (std::abs(stick.x) >= std::abs(stick.y)) {
            stick.x = toward_zero(stick.x);
        } else {
            stick.y = toward_zero(stick.y);
        }
    }
    return stick;
}

}  // namespace a11y
