/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "voice_settings.hpp"
#include "voice_names.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <utility>

namespace a11y {
namespace {

bool touches(const std::vector<const NamedValue*>& changed, const NamedValue& named) {
    return std::find(changed.begin(), changed.end(), &named) != changed.end();
}

/* A Rate row's value, 0 to 100, as the voices' rate, 0 to 1, kNormalRate
 * the voice's normal speed. */
float bridge_rate(const Number& rate) {
    return rate.value() / 100.0f;
}

}  // namespace

ChoiceOptions voice_options(const std::vector<VoiceInfo>& voices) {
    const bool languages = std::any_of(voices.begin(), voices.end(),
        [&voices](const VoiceInfo& voice) { return voice.language != voices.front().language; });
    ChoiceOptions options;
    for (const VoiceInfo& voice : voices) {
        ChoiceOption option{voice.name, ""};
        if (languages) {
            option.label = voice.name + ", " + language_name(voice.language);
        }
        options.push_back(std::move(option));
    }
    return options;
}

VoiceSettings::VoiceSettings(ValueModel& model, Speech& speech)
    : m_speech(speech),
      m_sample_delay(model.group("speech", "Speech")
              .add_number(NumberSpec{"voice_sample_delay", "Voice sample delay",
                  "How long a voice waits after a change to its Voice, Rate or Volume row "
                  "before it plays its sample, so the screen reader's first words come "
                  "through; a quicker run of changes plays one sample.",
                  "s", 0.6f, 0.0f, 3.0f, 0.1f})),
      m_rows{
          add_rows(model, "your_voice", "Your voice", "your voice", "Zira", "what happens to you"),
          add_rows(model, "opponents_voice", "Opponents' voice", "the opponents' voice", "Mark",
              "what happens to everyone else, teammates included,"),
      } {}

VoiceSettings::Rows VoiceSettings::add_rows(ValueModel& model, const char* id, const char* title,
    const char* in_sentence, const char* default_voice, const char* about) {
    ValueGroup group = model.group(id, title);
    const std::string name = title;
    const std::string inside = in_sentence;
    Choice voice = group.add_choice(ChoiceSpec{
        .name = "voice",
        .label = name,
        .description = std::string("The Windows voice that says ") + about + " in a match.",
        .default_option = default_voice,
        /* Called only once the rows exist: this is whole by then. */
        .list = [this] { return m_options; },
        .fallback = [this](std::string_view saved,
                        const ChoiceOptions& options) { return fallback(saved, options); },
        .none = [this] { return none(); },
        .shown = true,
    });
    Number rate = group.add_number(NumberSpec{"rate", name + " rate",
        "How fast " + inside + " speaks, 50 being the voice's normal speed.", "", 50.0f, 0.0f,
        100.0f, 5.0f, true});
    Number volume = group.add_number(NumberSpec{"volume", name + " volume",
        "The loudness of " + inside + ", under the master volume.", "%", 100.0f, 0.0f, 100.0f, 5.0f,
        true});
    /* "Preview opponents' voice", as "Preview gap warning". */
    std::string lowered = name;
    lowered[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[0])));
    group.add_action(ActionSpec{"preview", "Preview " + lowered,
        "Says \"" + name + "\" in " + inside + ", at its rate and volume."});
    return Rows{voice, rate, volume, id, name, std::nullopt};
}

void VoiceSettings::attach(Voices& voices) {
    m_voices = &voices;
    for (int i = 0; i < kVoiceCount; i++) {
        apply(static_cast<Voice>(i));
    }
}

void VoiceSettings::apply(Voice voice) {
    const Rows& r = rows(voice);
    m_voices->set_voice(voice, r.voice.saved());
    m_voices->set_rate(voice, bridge_rate(r.rate));
    m_voices->set_volume(voice, r.volume.value() / 100.0f);
}

void VoiceSettings::changed(const std::vector<const NamedValue*>& changed, Clock::time_point now) {
    if (m_voices == nullptr) {
        return; /* the load: attach applies it */
    }
    std::vector<Voice> renamed;
    for (int i = 0; i < kVoiceCount; i++) {
        const Voice voice = static_cast<Voice>(i);
        Rows& r = rows(voice);
        const bool voice_changed = touches(changed, r.voice.named());
        if (!voice_changed && !touches(changed, r.rate.named()) &&
            !touches(changed, r.volume.named()))
        {
            continue;
        }
        apply(voice);
        if (voice_changed) {
            renamed.push_back(voice);
        }
        r.sample_at = now + std::chrono::duration_cast<Clock::duration>(
                                std::chrono::duration<float>(m_sample_delay.value()));
    }
    if (m_told == VoicesState::ready) {
        /* A row lists only voices installed: a hand edit named this one. */
        const std::string said = missing(renamed);
        if (!said.empty()) {
            m_speech.announce(said, Mode::queue);
        }
    }
}

bool VoiceSettings::preview(std::string_view group) {
    for (int i = 0; i < kVoiceCount; i++) {
        const Voice voice = static_cast<Voice>(i);
        if (rows(voice).group == group) {
            sample(voice);
            return true;
        }
    }
    return false;
}

void VoiceSettings::frame(Clock::time_point now, bool launcher) {
    if (m_voices == nullptr) {
        return;
    }
    const VoicesState state = m_voices->state();
    if (m_told != state && state == VoicesState::ready) {
        m_told = state;
        m_options = voice_options(m_voices->installed());
        tell(missing({Voice::yours, Voice::opponents}), launcher);
    } else if (m_told != state && state == VoicesState::unavailable) {
        m_told = state;
        tell("Voices unavailable, using the screen reader.", launcher);
    }
    for (int i = 0; i < kVoiceCount; i++) {
        const Voice voice = static_cast<Voice>(i);
        const Rows& r = rows(voice);
        if (r.sample_at.has_value() && now >= *r.sample_at) {
            sample(voice);
        }
    }
}

void VoiceSettings::tell(std::string news, bool launcher) {
    if (news.empty()) {
        return;
    }
    if (launcher || m_scene_announced) {
        /* After the proof-of-life, which the voices' start follows. */
        m_speech.announce(news, Mode::queue);
    } else {
        m_news = std::move(news);
    }
}

void VoiceSettings::scene_announced() {
    m_scene_announced = true;
    if (!m_news.empty()) {
        m_speech.announce(std::exchange(m_news, {}), Mode::queue);
    }
}

void VoiceSettings::sample(Voice voice) {
    Rows& r = rows(voice);
    r.sample_at.reset();
    /* A sample still playing is cut by the new one: the same subject. */
    m_speech.announce_voice(voice, kPreviewSubject, r.sample);
}

std::string VoiceSettings::fallback(std::string_view saved, const ChoiceOptions& options) const {
    std::vector<VoiceInfo> names;
    int default_index = -1;
    for (const ChoiceOption& option : options) {
        if (m_voices != nullptr && option.text == m_voices->default_voice()) {
            default_index = static_cast<int>(names.size());
        }
        names.push_back(VoiceInfo{option.text, ""});
    }
    const int index = find_voice(names, saved, default_index);
    return index < 0 ? std::string() : options[static_cast<size_t>(index)].text;
}

std::string VoiceSettings::none() const {
    switch (m_voices != nullptr ? m_voices->state() : VoicesState::stopped) {
    case VoicesState::stopped:
        return "Off";
    case VoicesState::starting:
        return "Starting";
    case VoicesState::ready:
    case VoicesState::unavailable:
        break;
    }
    return "Unavailable";
}

std::string VoiceSettings::missing(const std::vector<Voice>& voices) const {
    std::vector<std::string> saved;
    std::vector<std::string> in_use;
    for (Voice voice : voices) {
        const std::string& text = rows(voice).voice.saved();
        if (text.empty() || match_voice(m_voices->installed(), text) >= 0 ||
            std::find(saved.begin(), saved.end(), text) != saved.end())
        {
            continue;
        }
        saved.push_back(text);
        in_use.push_back(m_voices->voice_in_use(voice));
    }
    if (saved.empty()) {
        return {};
    }
    /* Both fall back to the Windows default voice: one sentence. */
    if (saved.size() == 2 && in_use[0] == in_use[1]) {
        return saved[0] + " and " + saved[1] + " not installed, using " + in_use[0] + ".";
    }
    std::string said;
    for (size_t i = 0; i < saved.size(); i++) {
        if (!said.empty()) {
            said += " ";
        }
        said += saved[i] + " not installed, using " + in_use[i] + ".";
    }
    return said;
}

}  // namespace a11y
