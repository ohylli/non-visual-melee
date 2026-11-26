/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cues of a match (primer docs/a11y/cues.md, "The match cues"): each
 * registers its values with the cue settings and its voices with the cue
 * engine once, then turns every tick's match snapshot into its voices'
 * targets through its driver. Each also has a preview, a short sample
 * played outside any match on voices of its own ("Preview"). The items new
 * on each tick, which the appearance cue plays, are kept here for the
 * appearance announcement too. Game thread only. */
#pragma once
#include "cue_engine.hpp"
#include "cue_settings.hpp"
#include "gap_warning.hpp"
#include "game_access.h"
#include "item_appearance.hpp"
#include "item_frame.hpp"
#include "item_position.hpp"
#include "ledge_warning.hpp"
#include "position_cue.hpp"
#include <array>
#include <chrono>
#include <optional>
#include <span>
#include <string_view>

namespace a11y {

using Clock = std::chrono::steady_clock;

/* The sounds of the cues that play files (cue_sounds.hpp, load_sound), each
 * empty when its file is missing, which leaves that cue silent. */
struct MatchCueSounds {
    Sound item_appearance;
    Sound item_position;
};

class MatchCues {
public:
    /* The shortest fast interval the settings allow, which your cue's tick
     * is made to fit: as long at every pitch, it ends inside the fastest
     * pulse. The widest pitch range, half of it down and half up, which
     * the position cues' and item cues' sounds have copies across. */
    static constexpr float kFastestInterval = 0.07f;
    static constexpr float kWidestPitchRange = 2.0f;
    /* The shortest interval the opponent's cue allows, which its longer
     * pulse fits the same way. */
    static constexpr float kOpponentFastestInterval = 0.2f;
    /* The shortest within-reach interval the item's position cue allows. A
     * longer pulse is cut there with a fade (cue_engine.hpp). */
    static constexpr float kItemFastestInterval = 0.05f;
    /* How long a preview's voices, once stopped, wait before they start
     * again: past the stop ramp and an audio block, so a restarted preview
     * starts from silence, its first pulse at once, rather than carrying on
     * the old rhythm, and the restart is heard as one. */
    static constexpr std::chrono::milliseconds kPreviewRestart{100};

    /* The preview's voices in the engine, apart from the match's, each -1
     * when refused. */
    struct PreviewVoices {
        int your = -1;
        int opponent = -1;
        int gap = -1;
        int ledge_left = -1;
        int ledge_right = -1;
        int item_position = -1;
    };

    /* Before the accessibility settings file is read, so the file gains
     * these values. */
    MatchCues(CueSettings& settings, CueEngine& engine, MatchCueSounds sounds = {});

    /* After every simulated tick, not while rollback re-runs ticks: starts,
     * updates or stops each voice. */
    void tick(const A11yMatchState& match);
    /* Every voice stops with its ramp, and what the drivers keep between
     * ticks (the opponent's side, the items' sides, the gap warning's hold)
     * is forgotten, so the next tick starts the cues afresh from its
     * snapshot, as after any silence. Called each frame the port menu is
     * open, while no tick runs. */
    void silence();
    /* A scene was entered: silence, and the items seen and held are
     * forgotten, since each match is a scene and counts its items' serials
     * from 0. */
    void scene_entered();
    /* The items new on the last tick, nearest to you first, for the
     * appearance announcement. */
    std::span<const A11yMatchItem> items_appeared() const { return m_new_items.appeared(); }

    /* Starts the preview of the cue with this id, the one add_cue took
     * ("gap_warning"), from its beginning: about two seconds of the cue at
     * a fixed, typical moment of a match, at its current values. Its voices
     * ignore the switches and keep every volume, and neither silence nor a
     * tick touches them. Again restarts it; another cue's stops the one
     * playing first. False for an id with no preview. */
    bool preview(std::string_view id, Clock::time_point now);
    static bool has_preview(std::string_view id) { return preview_named(id) != Preview::none; }
    /* The preview stops, with the engine's ramp; nothing when none plays. */
    void stop_preview(Clock::time_point now);
    /* A preview plays, or waits kPreviewRestart to start. */
    bool previewing() const { return m_preview != Preview::none; }
    /* Each frame, wherever the game is: the preview's voices follow its
     * moment, and stop at its end. */
    void preview_frame(Clock::time_point now);
    const PreviewVoices& preview_voices() const { return m_preview_voices; }

    /* The voices' numbers in the engine, -1 for one it refused. */
    int your_voice() const { return m_your_voice; }
    int opponent_voice() const { return m_opponent_voice; }
    int gap_voice() const { return m_gap_voice; }
    int ledge_left_voice() const { return m_ledge_left_voice; }
    int ledge_right_voice() const { return m_ledge_right_voice; }
    /* The appearance cue's one-shots, and its preview's; empty when its
     * sound was missing. */
    std::span<const int> appearance_voices() const { return m_appearance_pool.voices(); }
    std::span<const int> appearance_preview_voices() const {
        return m_appearance_preview_pool.voices();
    }
    /* The item's position cue's voices, the nearest item's first, each -1
     * when refused (its sound missing). */
    std::span<const int> item_position_voices() const { return m_item_position_voices; }

    /* How far past each end of the floor below you the snapshot's deadly
     * test casts (A11yFloorEnd), as the ledge warning's settings hold it. */
    float edge_offset() const { return m_ledge.edge_offset.value(); }
    /* Position cues follow the camera, for positional sound effects on the
     * stage to pan as the position cues do. */
    bool pan_to_camera() const { return m_pan_to_camera.on(); }

private:
    enum class Preview { none, your, opponent, gap, ledge, appearance, item_position };

    struct StageValues {
        Number pitch_range;

        StageMapping mapping(bool pan_to_camera) const;
    };
    struct YourValues {
        StageValues stage;
        Number slow_interval;
        Number fast_interval;
        Number far_distance;
        Number near_distance;

        YourMapping mapping(bool pan_to_camera) const;
    };
    struct OpponentValues {
        StageValues stage;
        Number interval;
        Switch relative;
        Number side_distance;
        Number pan_floor;
        Number crossing_margin;
        Number height_distance;

        OpponentMapping mapping(bool pan_to_camera) const;
    };
    struct GapValues {
        Number hold;

        GapMapping mapping() const;
    };
    /* The item cues' frame, shared by both item cues. */
    struct ItemValues {
        Switch relative;
        Number pitch_range;
        Number side_distance;
        Number pan_floor;
        Number height_distance;
        /* The item's position cue's alone: a one-shot has no side to
         * keep. */
        Number crossing_margin;

        ItemMapping mapping(bool pan_to_camera) const;
    };
    struct ItemPositionValues {
        Number pitch_range;
        Number steady_interval;
        Number reach_interval;

        /* The item cues' frame with the cue's own pitch range. */
        ItemPositionMapping mapping(ItemMapping frame) const;
    };
    struct LedgeValues {
        Number distance;
        Number pan_floor;
        Number level_floor;
        Number edge_offset;

        LedgeMapping mapping() const;
    };

    static StageValues add_stage_values(ValueGroup group);
    static YourValues add_your_values(ValueGroup group);
    static OpponentValues add_opponent_values(ValueGroup group);
    static ItemValues add_item_values(ValueGroup group);
    static ItemPositionValues add_item_position_values(ValueGroup group);
    static GapValues add_gap_values(ValueGroup group);
    static LedgeValues add_ledge_values(ValueGroup group);
    /* The preview of the cue with this id, none for an unknown one. */
    static Preview preview_named(std::string_view id);
    /* A voice with targets starts, or only moves when it plays; one without
     * stops. */
    void play(int voice, const std::optional<VoiceTargets>& targets);
    /* The item's position cue's voice `i` follows its item, or stops with
     * none; a change of reach while it plays is logged. */
    void follow_item(std::size_t i, const std::optional<TrackedItem>& item);
    /* The appearance cue's preview at its moment: false once it has
     * ended. */
    bool appearance_preview_frame(double seconds, bool pan_to_camera);

    CueEngine& m_engine;
    /* In the order the accessibility settings list them, in the file and
     * in the menus: each cue registers its group and its values; the value
     * both position cues share after the opponent's position cue, so its
     * row sits beside theirs; the item's position cue after the item
     * appearance cue, then the item cues' frame last, so Items relative to
     * you follows both item cues' rows. */
    CueGroup m_your_cue;
    YourValues m_your;
    CueGroup m_opponent_cue;
    OpponentValues m_opponent;
    OpponentPosition m_opponent_position;
    Switch m_pan_to_camera;
    CueGroup m_gap_cue;
    GapValues m_gap;
    GapWarning m_gap_warning;
    CueGroup m_ledge_cue;
    LedgeValues m_ledge;
    CueGroup m_appearance_cue;
    NewItems m_new_items;
    AppearanceCue m_appearance;
    OneShotPool m_appearance_pool;
    OneShotPool m_appearance_preview_pool;
    /* The appearance cue's sound's length, for its preview's end. */
    double m_appearance_seconds = 0.0;
    CueGroup m_item_position_cue;
    ItemPositionValues m_item_position_values;
    ItemValues m_items;
    ItemPosition m_item_position;
    std::array<int, kTrackedItems> m_item_position_voices{};
    /* Each voice's item within reach, as last started or moved. */
    std::array<bool, kTrackedItems> m_item_within_reach{};
    int m_your_voice;
    int m_opponent_voice;
    int m_gap_voice;
    int m_ledge_left_voice;
    int m_ledge_right_voice;
    int m_tick = 0;
    PreviewVoices m_preview_voices;
    Preview m_preview = Preview::none;
    /* The appearance cue's preview plays triggered so far. */
    std::size_t m_preview_played = 0;
    /* When the preview's moment begins: later than its start call while its
     * voices wait out kPreviewRestart. */
    Clock::time_point m_preview_start;
    /* Until when stopped preview voices wait. */
    Clock::time_point m_preview_quiet;
};

}  // namespace a11y
