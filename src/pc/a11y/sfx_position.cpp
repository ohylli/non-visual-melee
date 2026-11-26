/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sfx_position.hpp"
#include "pc/pc.h"
#include "position_cue.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace a11y {

namespace {

/* The first track of each block of fighter tracks the game uses
 * (ft_0881.c: fp->player_idx * 2 + base + is_sub_fighter), 0x7E the hits a
 * fighter takes (ftColl_80078384 in ftcoll.c). */
constexpr std::array<int, 8> kTrackBases{0x1E, 0x2A, 0x36, 0x42, 0x4E, 0x5A, 0x72, 0x7E};
constexpr int kTracksPerBlock = A11Y_MATCH_SLOTS * 2;

/* A followed voice is panned again once its place has moved this far in
 * the engine's steps, so a still fighter's sounds are left alone. */
constexpr int kRepanStep = 2;

/* The engine's voice pool (AXDriver_804C45A0): no more can play. */
constexpr std::size_t kMostFollowed = 0x60;

/* A start's volume scaled by its place's gain. */
std::uint8_t scaled(std::uint8_t volume, float gain) {
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(static_cast<float>(volume) * gain), 0L, 255L));
}

/* Logged per voice placed: its track, 0x00 for one named by
 * fighter_next. */
void log_placed(int track, SfxSource source, std::uint8_t volume, const SfxPlace& place) {
    pc_log_line("[a11y] sfx placed: track 0x%02X slot %d%s pan %d volume %d gain %.2f", track,
        source.slot, source.index == 1 ? " sub" : "", place.pan, volume, place.gain);
}

}  // namespace

std::optional<SfxSource> sfx_track_source(int track) {
    for (int base : kTrackBases) {
        const int offset = track - base;
        if (offset >= 0 && offset < kTracksPerBlock) {
            return SfxSource{offset / 2, offset % 2};
        }
    }
    return std::nullopt;
}

std::uint8_t sfx_engine_pan(float pan) {
    const float clamped = std::clamp(pan, -1.0f, 1.0f);
    /* 0x80 is the engine's centre: -1 reaches 0 and 1 reaches 0xFF. */
    const float scaled = clamped < 0.0f ? 128.0f * (1.0f + clamped) : 128.0f + 127.0f * clamped;
    return static_cast<std::uint8_t>(std::lround(scaled));
}

float sfx_distance_gain(float distance, const SfxMapping& mapping) {
    const float far_level = std::clamp(mapping.far_level, 0.0f, 1.0f);
    if (distance <= mapping.near_distance || far_level >= 1.0f) {
        return 1.0f;
    }
    if (distance >= mapping.far_distance || mapping.far_distance <= mapping.near_distance) {
        return far_level;
    }
    if (far_level <= 0.0f) {
        /* No decibel scale reaches silence: linear in the level instead. */
        return 1.0f -
               (distance - mapping.near_distance) / (mapping.far_distance - mapping.near_distance);
    }
    const float t =
        (distance - mapping.near_distance) / (mapping.far_distance - mapping.near_distance);
    return std::pow(far_level, t);
}

SfxPlace sfx_place(const A11yStageFrame& stage, float you_x, float you_y, float x, float y,
    const SfxMapping& mapping) {
    const float dx = x - you_x;
    const float dy = y - you_y;
    SfxPlace place;
    if (mapping.relative) {
        const float side = std::max(mapping.side_distance, 1.0f);
        place.pan = sfx_engine_pan(dx / side);
    } else {
        place.pan = sfx_engine_pan(stage_pan(stage, x, mapping.pan_to_camera));
    }
    place.gain = sfx_distance_gain(std::sqrt(dx * dx + dy * dy), mapping);
    return place;
}

void SfxPositions::tick(bool on, const A11yMatchState& match, const A11ySfxSources& sources,
    const SfxMapping& mapping) {
    /* A name or a hint the last tick left unused belongs to no start. */
    m_named.reset();
    m_hint = -1;
    if (!on || !match.running) {
        forget();
        return;
    }
    m_placing = true;
    for (int slot = 0; slot < A11Y_MATCH_SLOTS; slot++) {
        for (int index = 0; index < 2; index++) {
            const A11ySfxFighter& f = sources.fighters[slot][index];
            std::optional<SfxPlace>& place = m_places[slot][index];
            if (!f.present) {
                place.reset();
            } else if (slot == match.you.slot && index == 0 && mapping.relative) {
                /* Your own sounds, from you: centred at their own volume. */
                place = SfxPlace{};
            } else {
                place = sfx_place(match.stage, match.you.x, match.you.y, f.x, f.y, mapping);
                if (slot == match.you.slot && index == 0) {
                    place->gain = 1.0f;
                }
            }
        }
    }
    /* Follow each voice to its fighter, dropping those that have ended. */
    std::erase_if(m_voices, [&](Followed& v) {
        const std::optional<SfxPlace>& place = m_places[v.source.slot][v.source.index];
        if (place.has_value() && std::abs(place->pan - v.pan) >= kRepanStep) {
            v.pan = place->pan;
            m_engine.remember(v.voice, v.pan);
        }
        /* Set every tick, the pan unchanged included, so an ended voice is
         * let go. */
        return !m_engine.set_pan(v.voice, v.pan);
    });
}

void SfxPositions::started(
    int voice, int track, std::uint8_t* volume, std::uint8_t* pan, std::uint8_t* script_pan) {
    /* Whose: its own track, else the track a hit asked for, else the fighter
     * named just before it. Each is good for this start alone. */
    std::optional<SfxSource> source = sfx_track_source(track);
    if (!source.has_value() && m_hint >= 0) {
        source = sfx_track_source(m_hint);
        track = m_hint;
    }
    if (!source.has_value() && m_named.has_value()) {
        source = m_named;
        track = 0;
    }
    m_hint = -1;
    m_named.reset();
    if (!m_placing || !source.has_value()) {
        return;
    }
    const std::optional<SfxPlace> at = place(*source);
    if (!at.has_value()) {
        return;
    }
    *volume = scaled(*volume, at->gain);
    /* The pan the first layer takes, and the one its script starts each
     * later layer from (the engine forgets the first after the first
     * layer), so every layer of the sound is placed alike. */
    *pan = at->pan;
    *script_pan = at->pan;
    m_engine.remember(voice, at->pan);
    log_placed(track, *source, *volume, *at);
    follow(voice, *source, at->pan);
}

void SfxPositions::follow(int voice, SfxSource source, std::uint8_t pan) {
    if (m_voices.size() < kMostFollowed) {
        m_voices.push_back(Followed{voice, source, pan});
    }
}

void SfxPositions::forget() {
    m_placing = false;
    for (auto& row : m_places) {
        for (auto& place : row) {
            place.reset();
        }
    }
    m_voices.clear();
    m_named.reset();
    m_hint = -1;
}

std::optional<SfxPlace> SfxPositions::place(SfxSource source) const {
    if (!m_placing || source.slot < 0 || source.slot >= A11Y_MATCH_SLOTS || source.index < 0 ||
        source.index > 1)
    {
        return std::nullopt;
    }
    return m_places[source.slot][source.index];
}

}  // namespace a11y
