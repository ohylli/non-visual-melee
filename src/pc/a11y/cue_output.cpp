/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cue_output.hpp"
#include "cue_engine.hpp"
#include "pc/pc.h"
#include <SDL3/SDL.h>
#include <algorithm>

namespace a11y {
namespace {

/* Frames per render inside the callback. */
constexpr int kChunk = 256;

}  // namespace

std::unique_ptr<CueOutput> CueOutput::open(CueEngine& engine, bool log, std::string& why) {
    std::unique_ptr<CueOutput> output(new CueOutput(engine));
    /* Counted by SDL, so the game's own audio and this one start and stop
     * the subsystem independently. */
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        why = std::string("SDL audio did not start: ") + SDL_GetError();
        return nullptr;
    }
    output->m_audio_initialized = true;
    const SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, kCueRate};
    output->m_stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, pull, output.get());
    if (output->m_stream == nullptr) {
        why = std::string("no playback device: ") + SDL_GetError();
        return nullptr;
    }
    SDL_AudioSpec device = {};
    int device_frames = 0;
    SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(output->m_stream), &device, &device_frames);
    if (log) {
        pc_log_line("[a11y] cue engine: SDL driver %s, device %d Hz, %d-frame buffer",
            SDL_GetCurrentAudioDriver(), device.freq, device_frames);
    }
    if (!SDL_ResumeAudioStreamDevice(output->m_stream)) {
        why = std::string("the device did not start: ") + SDL_GetError();
        return nullptr;
    }
    return output;
}

CueOutput::~CueOutput() {
    if (m_stream != nullptr) {
        SDL_DestroyAudioStream(m_stream);
    }
    if (m_audio_initialized) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void CueOutput::pull(void* userdata, SDL_AudioStream* stream, int additional, int total) {
    (void)total;
    CueOutput& output = *static_cast<CueOutput*>(userdata);
    int frames = additional / static_cast<int>(sizeof(float) * 2);
    float buffer[kChunk * 2];
    while (frames > 0) {
        const int n = std::min(frames, kChunk);
        output.m_engine.render(buffer, n);
        SDL_PutAudioStreamData(stream, buffer, n * 2 * static_cast<int>(sizeof(float)));
        frames -= n;
    }
}

}  // namespace a11y
