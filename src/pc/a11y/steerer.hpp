/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A screen's steering as its reader runs it
 * (docs/adr/0004-free-cursors-are-steered-through-the-controller.md): the
 * glide, where it goes as the screen names its targets, and the stick it
 * asks for until the pad hook takes it. The reader decides where a step
 * goes, what the glide aims at and when steering is off; this keeps the
 * glide's bookkeeping, and logs every glide with its destination in the
 * screen's words. A screen's targets are its own type, so this is a
 * template over it. */
#pragma once
#include "game_access.h"
#include "speech.hpp"
#include "steering.hpp"
#include "steps.hpp"
#include <functional>
#include <string>
#include <utility>

namespace a11y {

/* How a glide stands, as a screen's speech follows it. */
enum class GlidePhase {
    none,
    /* Under way to its destination. */
    gliding,
    /* Ended since the last frame, at the destination or elsewhere. */
    ended,
    /* Ended since the last frame without getting there. */
    failed,
};

/* A glide as a screen's speech follows it, kept by its Steerer. */
template <typename Destination>
struct GlideState {
    using Phase = GlidePhase;
    Phase phase = Phase::none;
    Destination destination{};
};

/* The first direction pressed this frame on the pad, if any. */
bool pressed_direction(const A11yPad& pad, Direction* out);

/* Why a glide ended, for the log. */
const char* why_ended(GlideEnd end);

/* The log's lines for a glide to the destination named words: its start
 * towards aim, and its end after frames, for why. */
void log_glide_start(const std::string& words, Point aim);
void log_glide_end(const std::string& words, int frames, const char* why);

template <typename Destination>
class Steerer {
public:
    /* A destination as the screen names it now: "Fox", "Corneria". */
    using Words = std::function<std::string(const Destination&)>;

    /* rule must outlive this. */
    Steerer(const MovementRule& rule, Words words)
        : m_rule(&rule), m_glide(rule), m_words(std::move(words)) {}

    /* A scene was entered: no glide, nothing asked for. */
    void forget() {
        m_glide = Glide(*m_rule);
        m_status = GlideState<Destination>{};
        m_request_waiting = false;
    }

    bool active() const { return m_glide.active(); }
    /* The glide as speech follows it; an ended one until reported(). */
    const GlideState<Destination>& status() const { return m_status; }
    /* The frame hook has handed status() to the speech: a glide's end is
     * reported once. */
    void reported() {
        if (m_status.phase != GlidePhase::gliding) {
            m_status = GlideState<Destination>{};
        }
    }

    /* Glides to the destination, at aim, from wherever the cursor is, or
     * turns a glide under way there. */
    void start(const Destination& to, Point aim) {
        m_status = GlideState<Destination>{GlidePhase::gliding, to};
        m_glide.start(aim);
        if (log_enabled()) {
            log_glide_start(m_words(to), aim);
        }
    }

    /* The destination moved to aim; the glide goes on. */
    void retarget(Point aim) { m_glide.retarget(aim); }

    /* Stops a glide under way, for why: failed, or ended elsewhere. */
    void stop(const char* why, bool failed) {
        m_glide.stop();
        ended(failed, why);
    }

    /* The Steering switch went off: a glide under way ends where the
     * cursor is, for why, and nothing more is asked for, so the next pad
     * the hook publishes has the stick at rest (spec open question 6). The
     * glide starts afresh, since the frames it misses meanwhile would leave
     * its count of the sticks on their way wrong. */
    void switch_off(const char* why) {
        if (m_glide.active()) {
            stop(why, false);
        }
        m_glide = Glide(*m_rule);
        m_request_waiting = false;
    }

    /* Once per simulated frame, gliding or not, so the glide keeps count of
     * the sticks still on their way: where the cursor is after this frame's
     * movement, and the stick the game applied, as the pad held it. */
    void frame(Point cursor, Stick applied) {
        GlideFrame out = m_glide.frame(cursor, applied);
        m_request = out.stick;
        m_request_waiting = m_rule->moves(out.stick);
        if (out.end != GlideEnd::none) {
            ended(out.end == GlideEnd::failed, why_ended(out.end));
        }
    }

    /* The pad hook, once a video frame: the stick the glide asks for,
     * handed over once; false when it asks for none, so the stick returns
     * to rest by itself. */
    bool take_stick(Stick* out) {
        m_glide.published();
        if (!m_request_waiting) {
            return false;
        }
        m_request_waiting = false;
        *out = m_request;
        return true;
    }

private:
    void ended(bool failed, const char* why) {
        if (log_enabled()) {
            log_glide_end(m_words(m_status.destination), m_glide.frames(), why);
        }
        m_status.phase = failed ? GlidePhase::failed : GlidePhase::ended;
    }

    const MovementRule* m_rule;
    Glide m_glide;
    Words m_words;
    GlideState<Destination> m_status;
    /* The stick the glide asks for, until the pad hook takes it. */
    Stick m_request;
    bool m_request_waiting = false;
};

}  // namespace a11y
