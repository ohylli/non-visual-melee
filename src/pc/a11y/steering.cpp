/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "steering.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace a11y {
namespace {

/* The game's cursor movement (getStickDelta in src/melee/mn/mncharsel.c). */
constexpr float kMoveThreshold = 200.0f;
constexpr float kSpeed = 0.0002f;
/* A full push, and the game's clamping (HSD_PadClamp, set up in gmmain.c):
 * a stick further out than this is scaled back to it, anything within it
 * arrives as sent. */
constexpr int kFullPush = 80;
constexpr float kFullStep = kSpeed * (static_cast<float>(kFullPush * kFullPush) - kMoveThreshold);
/* Online, a stick axis of 2 or less travels as 0 (at_rest in
 * src/pc/net_wire.c), so the glide never asks for one. */
constexpr int kWireRest = 2;
/* Two axis values this near rest count as the same: a resting stick's
 * drift, merged into the glide's by the pad merge, is no sign of the player
 * steering. Below this on both axes a stick barely moves a cursor. */
constexpr int kRestSlack = 14;

/* Nearer the destination than this, the glide asks for nothing more: the
 * smallest tilt that moves a cursor moves it 0.005, and every target is
 * several units wide. */
constexpr float kAimTolerance = 0.05f;
/* A sent stick not applied this many frames after the measured delay was
 * lost: a video frame that simulated nothing, or two that simulated one. */
constexpr int kLateSlack = 2;
/* A glide longer than this has failed: the longest takes about 60 frames at
 * the longest input delay. */
constexpr int kGiveUpFrames = 180;
/* How long a sent stick is still recognised as the glide's own. */
constexpr int kSentHistory = 60;

bool same_axis(int a, int b) {
    return a == b || (std::abs(a) <= kRestSlack && std::abs(b) <= kRestSlack);
}

bool same(Stick a, Stick b) {
    return same_axis(a.x, b.x) && same_axis(a.y, b.y);
}

int toward_zero(int value) {
    return value > 0 ? value - 1 : value + 1;
}

}  // namespace

Point cursor_step(Stick stick) {
    /* The game's own arithmetic, in floats, so a simulated hand lands where
     * the game's does. */
    float x = static_cast<float>(stick.x);
    float y = static_cast<float>(stick.y);
    float tilt_squared = x * x + y * y;
    if (tilt_squared < kMoveThreshold) {
        return Point{};
    }
    float speed = tilt_squared - kMoveThreshold;
    float angle = std::atan2(x, y);
    return Point{kSpeed * (speed * std::sin(angle)), kSpeed * (speed * std::cos(angle))};
}

bool moves(Stick stick) {
    return static_cast<float>(stick.x * stick.x + stick.y * stick.y) > kMoveThreshold;
}

void Glide::start(Point destination) {
    m_destination = destination;
    m_active = true;
    m_started = m_now;
}

GlideFrame Glide::frame(Point hand, Stick applied) {
    m_now++;
    m_samples_of_pad++;
    if (m_on_pad && m_samples_of_pad > 1) {
        /* A second simulated frame in one video frame sampled the pad
         * again: its stick moves the hand once more, a frame later. */
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
    /* Where the hand comes to rest if nothing more is asked for: the sticks
     * on their way still move it. */
    Point rest = hand;
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
    m_pad = Sent{m_request, m_now, cursor_step(m_request)};
    m_in_flight.push_back(m_pad);
    m_sent.push_back(m_pad);
}

bool Glide::account(Stick applied) {
    if (!moves(applied)) {
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
    /* The tilt that moves the hand the whole distance in one frame, at most
     * a full push; each frame asks afresh, so rounding is corrected as the
     * hand goes. */
    float length = std::hypot(distance.x, distance.y);
    float step = std::min(length, kFullStep);
    float tilt = std::sqrt(step / kSpeed + kMoveThreshold);
    Stick stick{static_cast<int>(std::lround(tilt * distance.x / length)),
        static_cast<int>(std::lround(tilt * distance.y / length))};
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
