/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The cue engine's mix (docs/adr/0005-cues-play-through-the-forks-own-audio-stream.md,
 * primer docs/a11y/cues.md): voices that keep sounding while the game thread
 * changes their targets every tick, rendered by the audio thread into a
 * stereo buffer. No device here: cue_output.cpp plays the mix through SDL,
 * and the tests render it straight into a buffer.
 *
 * Threads: the game thread adds voices, starts and stops them and writes
 * their targets and the volume chain's inputs; the audio thread only reads
 * them, in render. Nothing is locked: each value is an atomic, and a voice
 * is published to the audio thread whole once it is added. */
#pragma once
#include <array>
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

namespace a11y {

/* The mix's rate, and the rate every sound is made at. */
constexpr int kCueRate = 48000;

/* A cue's sound: mono samples at kCueRate, made by code (cue_sounds.hpp) or,
 * later, decoded from a file. */
struct Sound {
    std::vector<float> samples;

    double seconds() const { return static_cast<double>(samples.size()) / kCueRate; }
};

enum class VoiceMode {
    looped, /* the sound over and over, without a seam */
    pulsed, /* the sound once every interval, silence between */
    /* A sound handed over at run time by play, played once from its start:
     * a voice's line (voices.hpp). Added with no sound; centred unless its
     * pan says otherwise, at pitch 1. Its chain is the master volume × its
     * gain target alone: the mix and cue levels, the cue volume and every
     * cue switch leave it alone. */
    once,
};

/* What a voice should sound like now. The audio thread glides to each new
 * value over a few milliseconds, except the interval, which the next pulse
 * takes as it is. */
struct VoiceTargets {
    float pan = 0.0f;   /* -1 full left, 1 full right */
    float pitch = 1.0f; /* playback rate: 2 is an octave up */
    /* The voice's own factor of the volume chain: 0 to 1 for a cue's voice,
     * up to CueEngine::kMostBoost for a once voice, whose boost it carries. */
    float gain = 1.0f;
    float interval = 0.5f; /* pulsed: seconds from one pulse's start to the next's */
};

struct VoiceSpec {
    std::string name; /* in log and trace lines: "ledge right" */
    int cue = 0;      /* the cue whose level it takes, 0 to kMaxCues - 1 */
    Sound sound;      /* none for a once voice */
    VoiceMode mode = VoiceMode::looped;
    /* Targets are held to these. A pulsed voice's sound must end inside its
     * fastest interval even at its lowest pitch, where it plays longest, or
     * every pulse would be cut by the next: such a voice is refused. */
    float fastest_interval = 0.05f;
    float lowest_pitch = 0.5f;
    /* Pulsed: a pulsed voice added earlier, not yet paired, whose interval
     * this one shares. A voice of the two starting from silence while the
     * other sounds pulses halfway between the other's pulses, so the two
     * alternate rather than land together; started on the same tick, the
     * later added one waits half an interval, unless an audio block falls
     * between the two starts. -1 for none. */
    int alternates_with = -1;
    /* Sounds whatever the mix's and its cue's switches say, at their
     * volumes and boost and under the master volume: a cue's preview, which
     * plays with the cues turned off. */
    bool ignores_switches = false;
};

/* One input of the volume chain: the cue mix as a whole (cue volume and the
 * all-cues switch), or one cue (its switch, volume and boost). Off is
 * silence, reached with a glide, the voices still running underneath; a
 * voice that ignores the switches takes the volume and boost alone. */
struct CueLevel {
    bool on = true;
    float volume = 1.0f; /* 0 to 1 */
    /* Under the volume: a short click reads quieter than a held tone at the
     * same level, so its cue can be lifted without a slider past 100%. */
    float boost = 1.0f;
};

struct CueEngineOptions {
    bool log = true;   /* the speech log switch: start, stop and refusal lines */
    std::string dump;  /* MELEE_A11Y_CUE_DUMP: the mix, raw f32 stereo at kCueRate */
    std::string trace; /* MELEE_A11Y_CUE_TRACE: every voice's targets each tick */
};

class CueEngine {
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kMaxCues = 16;
    /* The largest boost a cue level takes; cue settings offer up to it. */
    static constexpr float kMostBoost = 8.0f;

    explicit CueEngine(CueEngineOptions options = {});
    ~CueEngine();
    CueEngine(const CueEngine&) = delete;
    CueEngine& operator=(const CueEngine&) = delete;

    /* Game thread. A voice is added once, stopped, and kept for the
     * engine's life; the result is its number, or -1 with a log line when
     * it is refused. */
    int add_voice(VoiceSpec spec);
    /* Starting a voice that plays changes only its targets; a pulsed voice
     * started from silence pulses at once. Each start and stop is logged
     * with the voice's targets. A once voice is not started: play starts
     * it. */
    void start(int voice, const VoiceTargets& targets);
    void set_targets(int voice, const VoiceTargets& targets);
    /* A once voice's stop ramps its sound out and drops one handed over and
     * not yet begun; neither is logged. */
    void stop(int voice);
    void stop_all();
    /* A once voice plays from play until its sound has played out or
     * ramped out, as the audio thread finds. */
    bool playing(int voice) const;
    /* A once voice's next sound, at its targets. It begins at once,
     * ramping in; one sounding is cut with the stop ramp first, and one
     * handed over earlier and not yet begun is dropped. False, with a log
     * line, for a voice that is not a once voice. Not logged otherwise: the
     * voices log their own lines. */
    bool play(int voice, Sound sound);
    /* The volume chain: master × mix level × the voice's cue level × the
     * voice's gain target. Master is the launcher's volume with mute
     * applied. */
    void set_master_volume(float volume);
    void set_mix_level(const CueLevel& level);
    void set_cue_level(int cue, const CueLevel& level);
    /* Once a tick, after the targets are set: one trace line per voice.
     * Nothing without a trace file. */
    void trace_tick(int tick);

    /* Audio thread: the next frames of the mix, interleaved stereo, also
     * written to the dump. */
    void render(float* out, int frames);

private:
    struct AtomicLevel {
        std::atomic<bool> on{true};
        std::atomic<float> volume{1.0f};
        std::atomic<float> boost{1.0f};

        void store(const CueLevel& level);
        /* Volume × boost, or 0 while off unless the switch is ignored. */
        float gain(bool ignore_switch) const;
    };
    /* A once voice's sounds, handed from the game thread to the audio
     * thread without a lock. A slot is free (the game thread may write it),
     * handed (in next, not yet taken), held (the audio thread reads it) or
     * done (the audio thread has finished with it; the game thread frees
     * it at its next play). Four are enough: one sounding, one waiting for
     * it to ramp out, one handed over, one being written. */
    static constexpr int kOnceSlots = 4;
    enum SlotState { kFree, kHanded, kHeld, kDone };
    struct Once {
        std::array<Sound, kOnceSlots> sounds;
        std::array<std::atomic<int>, kOnceSlots> state{};
        /* The slot handed over and not yet taken, or -1. */
        std::atomic<int> next{-1};
        /* Game thread: stops asked for, counted. */
        std::atomic<int> stops{0};
        /* Audio thread: a slot is held, sounding or waiting to. */
        std::atomic<bool> busy{false};
        /* Audio thread only. */
        int current = -1;  /* the slot sounding */
        int incoming = -1; /* the slot that begins once current ramps out */
        int stops_seen = 0;
        bool cutting = false; /* current ramps out with nothing after it */
    };
    struct Voice {
        /* Fixed once published. */
        VoiceSpec spec;
        /* Game thread to audio thread. */
        std::atomic<bool> on{false};
        std::atomic<float> pan{0.0f};
        std::atomic<float> pitch{1.0f};
        std::atomic<float> gain{1.0f};
        std::atomic<float> interval{0.5f};
        /* The voice it alternates with, both ways, or -1: set as voices are
         * added. */
        std::atomic<int> partner{-1};
        /* Game thread only: for its log and trace lines. */
        bool started = false;
        VoiceTargets targets;
        /* Audio thread only. */
        bool sounding = false; /* started, or still ramping down */
        double position = 0.0; /* in the sound, in frames */
        double since_pulse = 0.0;
        float ramp = 0.0f; /* the start and stop ramp, 0 to 1 */
        float pan_now = 0.0f;
        float pitch_now = 1.0f;
        float gain_now = 0.0f; /* the whole chain, glided */
        Once once;
    };

    Voice* voice_at(int voice);
    void write_targets(Voice& v, const VoiceTargets& targets);
    void log_voice(const Voice& v, const char* event) const;
    void render_voice(Voice& v, float chain, float* out, int frames);
    void render_once(Voice& v, float chain, float* out, int frames);
    void fall_between(Voice& v, double interval, int frames) const;

    CueEngineOptions m_options;
    std::array<Voice, kMaxVoices> m_voices;
    std::atomic<int> m_voice_count{0};
    std::atomic<float> m_master{1.0f};
    AtomicLevel m_mix;
    std::array<AtomicLevel, kMaxCues> m_cues;
    std::FILE* m_dump = nullptr;
    std::FILE* m_trace = nullptr;
};

}  // namespace a11y
