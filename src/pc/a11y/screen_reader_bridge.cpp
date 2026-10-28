/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "screen_reader_bridge.hpp"

/* Gated on the cmake-defined macro rather than the platform: Prism's C API is
 * the same everywhere it ships, so a new platform only needs its a11y.cmake
 * branch. Keep this file free of platform headers and platform calls. */
#if defined(A11Y_HAVE_PRISM)
#include <prism.h>

namespace a11y {
namespace {

class PrismBridge final : public ScreenReaderBridge {
public:
    ~PrismBridge() override { shutdown(); }

    std::string init() override {
        PrismConfig config = prism_config_init();
        m_context = prism_init(&config);
        if (m_context == nullptr) {
            return {};
        }
        /* create_best, not acquire_best: the fork has no reason to share
         * backend state. It ranks running screen readers first and falls back
         * to a system voice (OneCore, SAPI). It returns an initialised backend;
         * calling prism_backend_initialize again fails with "already
         * initialized". */
        m_backend = prism_registry_create_best(m_context);
        if (m_backend == nullptr) {
            return {};
        }
        const char* name = prism_backend_name(m_backend);
        return name != nullptr && name[0] != '\0' ? name : "unnamed backend";
    }

    void shutdown() override {
        if (m_backend != nullptr) {
            prism_backend_free(m_backend);
            m_backend = nullptr;
        }
        if (m_context != nullptr) {
            prism_shutdown(m_context);
            m_context = nullptr;
        }
    }

    std::string output(std::string_view utf8, bool interrupt) override {
        /* No backend was found at init; speech already logged that once. */
        if (m_backend == nullptr) {
            return {};
        }
        /* output rather than speak: it reaches braille displays too. */
        const std::string text(utf8);
        const PrismError error = prism_backend_output(m_backend, text.c_str(), interrupt);
        if (error == PRISM_OK) {
            return {};
        }
        const char* message = prism_error_string(error);
        return message != nullptr ? message : "unknown Prism error";
    }

private:
    PrismContext* m_context = nullptr;
    PrismBackend* m_backend = nullptr;
};

std::string error_text(PrismError error) {
    const char* message = prism_error_string(error);
    return message != nullptr ? message : "unknown Prism error";
}

/* The voices' instances. Prism is started here a second time, on the
 * voices' worker thread: prism_init initialises COM on the thread that calls
 * it as a single-threaded apartment, and the OneCore backend then moves its
 * blocking WinRT calls to the thread pool, so the worker needs nothing of its
 * own. A context of its own also keeps the screen reader's, on the game
 * thread, out of the worker's way. Measured with v0.18.2 in
 * docs/a11y/speech.md, "Voices". */
class PrismVoiceBridge final : public VoiceBridge {
public:
    ~PrismVoiceBridge() override { close(); }

    std::string open(int count) override {
        PrismConfig config = prism_config_init();
        m_context = prism_init(&config);
        if (m_context == nullptr) {
            return "Prism did not start";
        }
        for (int i = 0; i < count; i++) {
            PrismBackend* backend = prism_registry_create(m_context, PRISM_BACKEND_ONE_CORE);
            if (backend == nullptr) {
                return "no OneCore backend";
            }
            m_instances.push_back(backend);
            /* create, unlike create_best, leaves it uninitialised. */
            const PrismError error = prism_backend_initialize(backend);
            if (error != PRISM_OK) {
                return "OneCore did not start: " + error_text(error);
            }
        }
        return {};
    }

    /* The context is left to the process's end, never shut down: the voices
     * close only as the game exits, from its atexit handler, and there
     * prism_shutdown on the worker never returned once a OneCore instance
     * had rendered (v0.18.2; freeing the instances, or CoUninitialize alone,
     * returns). The worker's apartment ends with the thread. */
    void close() override {
        for (PrismBackend* backend : m_instances) {
            prism_backend_free(backend);
        }
        m_instances.clear();
        m_context = nullptr;
    }

    std::vector<VoiceInfo> voices() override {
        std::vector<VoiceInfo> found;
        PrismBackend* backend = instance(0);
        size_t count = 0;
        if (backend == nullptr || prism_backend_count_voices(backend, &count) != PRISM_OK) {
            return found;
        }
        for (size_t i = 0; i < count; i++) {
            VoiceInfo voice;
            /* Each text lives in the backend until its next call. */
            const char* text = nullptr;
            if (prism_backend_get_voice_name(backend, i, &text) == PRISM_OK && text != nullptr) {
                voice.name = text;
            }
            text = nullptr;
            if (prism_backend_get_voice_language(backend, i, &text) == PRISM_OK && text != nullptr)
            {
                voice.language = text;
            }
            found.push_back(std::move(voice));
        }
        return found;
    }

    int default_voice() override {
        /* Each instance starts at the Windows default voice. */
        PrismBackend* backend = instance(0);
        size_t index = 0;
        if (backend == nullptr || prism_backend_get_voice(backend, &index) != PRISM_OK) {
            return -1;
        }
        return static_cast<int>(index);
    }

    std::string set_voice(int which, int index) override {
        PrismBackend* backend = instance(which);
        if (backend == nullptr || index < 0) {
            return "no such voice";
        }
        const PrismError error = prism_backend_set_voice(backend, static_cast<size_t>(index));
        return error == PRISM_OK ? std::string() : error_text(error);
    }

    std::string set_rate(int which, float rate) override {
        PrismBackend* backend = instance(which);
        if (backend == nullptr) {
            return "no such instance";
        }
        const PrismError error = prism_backend_set_rate(backend, rate);
        return error == PRISM_OK ? std::string() : error_text(error);
    }

    std::string render(int which, std::string_view utf8, RenderedLine& out) override {
        PrismBackend* backend = instance(which);
        if (backend == nullptr) {
            return "no such instance";
        }
        out = RenderedLine{};
        const std::string text(utf8);
        /* OneCore calls back once, on this thread, before returning; Prism's
         * documentation allows chunks, so they are appended. */
        const PrismError error =
            prism_backend_speak_to_memory(backend, text.c_str(), collect, &out);
        return error == PRISM_OK ? std::string() : error_text(error);
    }

private:
    static void PRISM_CALL collect(
        void* userdata, const float* samples, size_t count, size_t channels, size_t rate) {
        RenderedLine& out = *static_cast<RenderedLine*>(userdata);
        out.samples.insert(out.samples.end(), samples, samples + count);
        out.channels = static_cast<int>(channels);
        out.rate = static_cast<int>(rate);
    }

    PrismBackend* instance(int which) const {
        if (which < 0 || which >= static_cast<int>(m_instances.size())) {
            return nullptr;
        }
        return m_instances[static_cast<size_t>(which)];
    }

    PrismContext* m_context = nullptr;
    std::vector<PrismBackend*> m_instances;
};

}  // namespace

std::unique_ptr<ScreenReaderBridge> make_screen_reader_bridge() {
    return std::make_unique<PrismBridge>();
}

std::unique_ptr<VoiceBridge> make_voice_bridge() {
    return std::make_unique<PrismVoiceBridge>();
}

}  // namespace a11y

#else

namespace a11y {
namespace {

/* No Prism on this platform yet: speech still logs every announcement. */
class SilentBridge final : public ScreenReaderBridge {
public:
    std::string init() override { return {}; }
    void shutdown() override {}
    std::string output(std::string_view, bool) override { return {}; }
};

/* No voices without Prism: open fails, and the voices say so. */
class NoVoiceBridge final : public VoiceBridge {
public:
    std::string open(int) override { return "no Prism on this platform"; }
    void close() override {}
    std::vector<VoiceInfo> voices() override { return {}; }
    int default_voice() override { return -1; }
    std::string set_voice(int, int) override { return "no voices"; }
    std::string set_rate(int, float) override { return "no voices"; }
    std::string render(int, std::string_view, RenderedLine&) override { return "no voices"; }
};

}  // namespace

std::unique_ptr<ScreenReaderBridge> make_screen_reader_bridge() {
    return std::make_unique<SilentBridge>();
}

std::unique_ptr<VoiceBridge> make_voice_bridge() {
    return std::make_unique<NoVoiceBridge>();
}

}  // namespace a11y

#endif
