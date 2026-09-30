/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Steering: moving a free cursor to a point by asking for stick input, the
 * way a player would (.scratch/character-select/spec.md, "Stage 2: the
 * glide"; docs/adr/0004-free-cursors-are-steered-through-the-controller.md).
 * The glide knows how the game moves a cursor for a stick value and nothing
 * about any screen. Pure: the hand's position and the stick the game applied
 * go in once per simulated frame, the stick to ask for comes out, and the
 * caller publishes it. */
#pragma once
#include <deque>

namespace a11y {

/* A stick value as the game reads it: each axis -80 to 80 at a full push. */
struct Stick {
    int x = 0;
    int y = 0;

    bool operator==(const Stick&) const = default;
};

/* A point in a screen's units, y growing upwards. */
struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

/* How far a free cursor moves in one frame for a stick value, as character
 * select's getStickDelta computes it: nothing below a squared tilt of 200,
 * else 0.0002 * (x² + y² - 200) units in the stick's direction. */
Point cursor_step(Stick stick);

/* The stick is pushed far enough to move a cursor. */
bool moves(Stick stick);

/* How a glide ended, reported once by the frame it ended in. */
enum class GlideEnd {
    none,
    /* The hand is at rest at the destination. */
    arrived,
    /* The hand did not get there in time. */
    failed,
    /* A stick the glide did not ask for moved the hand: the player's own. */
    abandoned,
};

struct GlideFrame {
    /* To ask for until the next simulated frame; {0, 0} asks for nothing. */
    Stick stick;
    GlideEnd end = GlideEnd::none;
};

class Glide {
public:
    /* Glides to destination from wherever the hand is, or turns a glide
     * under way there. */
    void start(Point destination);
    /* Ends the glide without reporting an end. */
    void stop() { m_active = false; }
    bool active() const { return m_active; }
    /* Simulated frames since the last start. */
    int frames() const { return m_now - m_started; }

    /* Once per simulated frame: where the hand is after this frame's
     * movement, and the stick the game applied to it this frame. */
    GlideFrame frame(Point hand, Stick applied);
    /* The pad was published, once a video frame: the stick the last frame
     * asked for, if any, went out with it, and the game will apply it some
     * frames on. A request not published before the next frame is dropped.
     * Every simulated frame until the next publish samples the same pad, so
     * a second one in the same video frame applies that stick again. */
    void published();

private:
    struct Sent {
        Stick stick;
        /* The frame that asked for it. */
        int frame = 0;
        /* How far it moves the hand. */
        Point step;
    };

    /* Matches what arrived against what was sent; false when a stick the
     * glide never asked for moved the hand. */
    bool account(Stick applied);
    Stick stick_for(Point distance) const;

    /* The input delay assumed until one is measured: longer than any online
     * input delay. */
    static constexpr int kUnmeasuredDelay = 30;

    bool m_active = false;
    Point m_destination;
    int m_now = 0;
    int m_started = 0;
    /* Frames from asking for a stick to the game applying it, as last
     * measured. */
    int m_delay = kUnmeasuredDelay;
    /* The stick the game applied last frame. */
    Stick m_last_applied;
    /* The stick the last frame asked for, until it is published. */
    Stick m_request;
    bool m_request_waiting = false;
    /* What the pad holds since it was last published, and the simulated
     * frames that have sampled it. */
    bool m_on_pad = false;
    Sent m_pad;
    int m_samples_of_pad = 0;
    /* Published and not yet applied, oldest first. */
    std::deque<Sent> m_in_flight;
    /* Everything published lately, applied or not: an applied stick found
     * here is the glide's own, early, late or repeated. */
    std::deque<Sent> m_sent;
};

}  // namespace a11y
