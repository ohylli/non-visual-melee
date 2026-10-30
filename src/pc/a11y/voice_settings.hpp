/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The voices' rows in the accessibility settings (.scratch/voices/spec.md,
 * "Choosing a voice", "The rows", "Missing voices"; primer
 * docs/a11y/speech.md, "Voices"): for your voice and then the opponents'
 * voice, a Voice row (a choice of the installed OneCore voices, saved by
 * display name), a Rate and a Volume slider, and Preview, in Voice settings
 * inside Match announcement settings, and a boost under the volume, a tuning
 * value. Every change
 * reaches the voices at once, from a row in either host or a hand edit of
 * a11y.ini, and a change to a voice's Voice, Rate, Volume or boost plays a
 * sample in it a moment later, one for a quick run of changes; Preview
 * plays it at once.
 *
 * A saved voice that is not installed stays in the file; the voice in use is
 * then the Windows default voice, which the row shows, and the screen reader
 * says so once the voices are ready, as it says once that they are
 * unavailable. Game thread only. */
#pragma once
#include "speech.hpp"
#include "value_model.hpp"
#include "voices.hpp"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

/* The Voice rows' options for these voices, sorted as Voices::installed
 * keeps them: each by its display name, with its language after it
 * ("Microsoft Mark, English (United States)") when the voices span more
 * than one language. */
ChoiceOptions voice_options(const std::vector<VoiceInfo>& voices);

class VoiceSettings {
public:
    /* Registers both voices' rows into `menu` (null: the top level),
     * Voice settings in the game (MatchAnnouncementSettings::voice_menu),
     * and the samples' delay with the Speech switch's group. */
    VoiceSettings(ValueModel& model, Speech& speech, const NamedMenu* menu = nullptr);
    VoiceSettings(const VoiceSettings&) = delete;
    VoiceSettings& operator=(const VoiceSettings&) = delete;

    /* Once the settings are loaded: the voices, which must outlive this,
     * take every value as it stands, with no sample. */
    void attach(Voices& voices);
    /* Every change to the settings after the load: the voices follow, and
     * a voice whose Voice, Rate, Volume or boost changed waits for its
     * sample. A voice not installed, which only a hand edit can name, is
     * said. */
    void changed(const std::vector<const NamedValue*>& changed, Clock::time_point now);
    /* A Preview row's press, by its action's group: true for a voice's,
     * whose sample plays at once. */
    bool preview(std::string_view group);
    /* Each frame, after the voices' frame (launcher: from the launcher's
     * frame hook): the rows' options once the voices are ready, and each
     * sample whose wait is over. As the voices are found ready or
     * unavailable, the screen reader is told what it needs to know: the
     * voices not installed, or the voices unavailable. The launcher queues
     * its opening behind that, so it is said at once there; in the game the
     * first screen's opening interrupts, so it waits for the first scene
     * announced. */
    void frame(Clock::time_point now, bool launcher);
    /* Once a scene's arrival has been announced (pc_a11y_scene_entered):
     * the news waiting for it is queued behind. */
    void scene_announced();

    Choice voice(Voice voice) const { return rows(voice).voice; }
    Number rate(Voice voice) const { return rows(voice).rate; }
    Number volume(Voice voice) const { return rows(voice).volume; }
    Number boost(Voice voice) const { return rows(voice).boost; }
    /* The sample's wait after a change, a tuning value in seconds. */
    float sample_delay() const { return m_sample_delay.value(); }

private:
    struct Rows {
        Choice voice;
        Number rate;
        Number volume;
        Number boost;      /* a factor, 1 to CueEngine::kMostBoost */
        std::string group; /* the Preview action's group: the rows' group id */
        std::string sample;
        std::optional<Clock::time_point> sample_at;
    };

    Rows& rows(Voice voice) { return m_rows[static_cast<size_t>(voice)]; }
    const Rows& rows(Voice voice) const { return m_rows[static_cast<size_t>(voice)]; }
    /* title as a row's name begins, in_sentence as it reads inside one
     * ("the opponents' voice"), about what the voice says. */
    Rows add_rows(ValueModel& model, const NamedMenu* menu, const char* id, const char* title,
        const char* in_sentence, const char* default_voice, const char* about);
    void apply(Voice voice);
    /* The option in use for a saved text the options hold none of: as
     * find_voice gives it, so the row shows the voice that speaks. */
    std::string fallback(std::string_view saved, const ChoiceOptions& options) const;
    std::string none() const;
    /* "Voice Zira not installed, using Microsoft David.", for the voices given
     * whose saved voice is not installed; empty when each is. */
    std::string missing(const std::vector<Voice>& voices) const;
    void sample(Voice voice);
    /* The voices' news, said now or once a scene is announced. */
    void tell(std::string news, bool launcher);

    Speech& m_speech;
    Voices* m_voices = nullptr;
    Number m_sample_delay;
    std::array<Rows, kVoiceCount> m_rows;
    /* The voices' options, kept for the rows' list: empty until ready. */
    ChoiceOptions m_options;
    /* The voices' state the news was last told for. */
    VoicesState m_told = VoicesState::stopped;
    bool m_scene_announced = false;
    std::string m_news; /* waiting for a scene's announcement */
};

}  // namespace a11y
