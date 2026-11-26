/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Positional sound effects (primer docs/a11y/cues.md, "Positional sound
 * effects"): the game plays every fighter's sounds centred, so a sighted
 * player's eyes do the placing. Here each fighter sound is panned to where
 * its fighter is, from you or across the stage, and played quieter the
 * further it is from you, so a blind player hears where an attack, a jump
 * or a landing happened. Your own fighter's sounds stay centred at their
 * own volume. Only the game's sound engine is touched, never the
 * simulation: a voice's volume as it starts and its pan while it plays.
 * Game thread only. */
#pragma once
#include "game_access.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace a11y {

/* A fighter's sounds as the game files them: by slot and whether the main
 * fighter or the sub fighter (Nana) made them. */
struct SfxSource {
    int slot = 0;
    int index = 0; /* 0 main, 1 sub */
    bool operator==(const SfxSource&) const = default;
};

/* The fighter whose sounds a track holds, or none for any other sound
 * (the stage's, an item's, a menu's). The game gives each fighter's sounds
 * tracks in blocks of twelve, two per slot, from base + slot * 2 + sub
 * (ft_0881.c), one block per kind of sound. */
std::optional<SfxSource> sfx_track_source(int track);

/* The mapping, as the settings hold it. */
struct SfxMapping {
    /* Pan from where the fighter is from you; off, from where it is across
     * the stage, as the position cues pan. */
    bool relative = true;
    /* Stage only: framed by the camera box rather than the blast zones. */
    bool pan_to_camera = true;
    /* Relative only: world units to your side where a sound is fully to
     * that side. */
    float side_distance = 100.0f;
    /* Within this distance of you (world units, straight line), a sound
     * plays at its own volume. */
    float near_distance = 15.0f;
    /* From this distance, and beyond, it plays at the far level. */
    float far_distance = 150.0f;
    /* The level at the far distance, 0 to 1 of the sound's own volume, so a
     * far sound is still heard. */
    float far_level = 0.35f;
};

/* Where a sound plays: the engine's pan, 0 left, 0x80 centre, 0xFF right,
 * and the factor its volume is scaled by. */
struct SfxPlace {
    std::uint8_t pan = 0x80;
    float gain = 1.0f;
    bool operator==(const SfxPlace&) const = default;
};

/* The engine's pan for a pan from -1 (left) to 1 (right). */
std::uint8_t sfx_engine_pan(float pan);

/* The level for a distance from you: 1 at the near distance and within,
 * the far level at the far distance and beyond, between them linear in
 * decibels, so each unit of distance quietens by the same ratio. */
float sfx_distance_gain(float distance, const SfxMapping& mapping);

/* A sound at (x, y) heard from you at (you_x, you_y) on the stage's
 * frame. */
SfxPlace sfx_place(const A11yStageFrame& stage, float you_x, float you_y, float x, float y,
    const SfxMapping& mapping);

/* Each simulated tick's places for the fighters' sounds, and the voices
 * started on fighter tracks, which keep following their fighter while they
 * play. */
class SfxPositions {
public:
    /* The game's sound engine, by its own voice ids: set_pan false once
     * the voice has ended; remember tells the engine's script hook where a
     * voice is placed (sfx_script_pan.hpp). */
    struct Engine {
        std::function<bool(int voice, std::uint8_t pan)> set_pan;
        std::function<void(int voice, std::uint8_t pan)> remember;
    };

    explicit SfxPositions(Engine engine) : m_engine(std::move(engine)) {}

    /* Once a tick, after the cues' snapshot: on is the setting with
     * Accessibility let through. Places every present fighter's sounds
     * from you, and pans each followed voice to its fighter's place where
     * that has moved. Outside a match, or off, every place is forgotten and
     * the voices are left to the game, so sounds start as the game made
     * them. */
    void tick(bool on, const A11yMatchState& match, const A11ySfxSources& sources,
        const SfxMapping& mapping);

    /* A sound's voice has started (pc_a11y_sfx_started), before it first
     * sounds: a fighter's, by its track, a hit's hint or the name given just
     * before it, has its volume scaled and its pan and script pan set, and
     * is followed. Any other sound is left as it is. */
    void started(
        int voice, int track, std::uint8_t* volume, std::uint8_t* pan, std::uint8_t* script_pan);

    /* The next start is this fighter's (pc_a11y_fighter_sfx, before a
     * start with no track of its own). Good for that start alone, and only
     * until the next tick. */
    void fighter_next(SfxSource source) { m_named = source; }

    /* The next start is a hit's whose caller asked for this track, which
     * the start drops (pc_a11y_sfx_track_hint): placed as if on it. Good
     * for that start alone, and only until the next tick. */
    void track_hint(int track) { m_hint = track; }

    /* A scene was entered: nothing is placed or followed until the next
     * tick of a match. */
    void forget();

    /* The place for a source's sounds, none when they are left alone. */
    std::optional<SfxPlace> place(SfxSource source) const;
    std::size_t followed() const { return m_voices.size(); }

private:
    struct Followed {
        int voice;
        SfxSource source;
        std::uint8_t pan;
    };

    void follow(int voice, SfxSource source, std::uint8_t pan);

    Engine m_engine;
    /* fighter_next's fighter, until the next start. */
    std::optional<SfxSource> m_named;
    /* track_hint's track, -1 for none. */
    int m_hint = -1;
    bool m_placing = false;
    /* By slot and index, valid while m_placing. */
    std::optional<SfxPlace> m_places[A11Y_MATCH_SLOTS][2];
    std::vector<Followed> m_voices;
};

}  // namespace a11y
