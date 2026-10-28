/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

/* The screen reader bridge: the only part of the fork that talks to the screen
 * reader library (Prism). Speech holds one and never sees the library itself;
 * the unit test swaps in a fake. */
class ScreenReaderBridge {
public:
    virtual ~ScreenReaderBridge() = default;
    /* Returns the backend name on success ("NVDA", "SAPI", ...), empty when no
     * backend could be created. */
    virtual std::string init() = 0;
    virtual void shutdown() = 0;
    /* interrupt=true cuts off current speech. Returns an error text on
     * failure, empty on success. */
    virtual std::string output(std::string_view utf8, bool interrupt) = 0;
};

/* Prism where a11y.cmake wired it in (A11Y_HAVE_PRISM), a silent stub elsewhere. */
std::unique_ptr<ScreenReaderBridge> make_screen_reader_bridge();

/* An installed voice as the voices' instances list it. */
struct VoiceInfo {
    std::string name;     /* the display name, "Microsoft Zira": a voice's only stable id */
    std::string language; /* "en-US" */

    bool operator==(const VoiceInfo&) const = default;
};

/* A line rendered to memory: interleaved samples, -1 to 1, in the format
 * the voice renders in, which can change with the voice. */
struct RenderedLine {
    std::vector<float> samples;
    int channels = 1;
    int rate = 0;
};

/* The voices' side of the bridge (docs/a11y/speech.md, "Voices"): Prism's
 * OneCore backend, one instance per voice, rendering lines to memory for the
 * cue engine to play. Every call comes from the voices' worker thread, the
 * one that opened it, as Prism's thread-safety rules ask; Prism is started
 * on that thread for the voices alone, apart from the screen reader's. The
 * unit test swaps in a fake. Each call that can fail returns an error text,
 * empty on success. */
class VoiceBridge {
public:
    virtual ~VoiceBridge() = default;
    /* Starts Prism on this thread and creates count instances, each at the
     * Windows default voice and its normal rate. */
    virtual std::string open(int count) = 0;
    /* Frees the instances, on the thread that opened them, as the game
     * exits. */
    virtual void close() = 0;
    /* The installed voices, in the instances' order, which the indexes below
     * follow; it may change between runs. */
    virtual std::vector<VoiceInfo> voices() = 0;
    /* The index of the Windows default voice, -1 when unknown. */
    virtual int default_voice() = 0;
    virtual std::string set_voice(int instance, int index) = 0;
    /* 0 to 1, 0.5 the voice's normal speed. */
    virtual std::string set_rate(int instance, float rate) = 0;
    /* The whole line, synthesized before it returns, silence trimmed at
     * both ends. */
    virtual std::string render(int instance, std::string_view utf8, RenderedLine& out) = 0;
};

/* Prism's OneCore backend where a11y.cmake wired Prism in, elsewhere a
 * bridge whose open fails. */
std::unique_ptr<VoiceBridge> make_voice_bridge();

}  // namespace a11y
