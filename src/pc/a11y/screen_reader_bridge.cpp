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

}  // namespace

std::unique_ptr<ScreenReaderBridge> make_screen_reader_bridge() {
    return std::make_unique<PrismBridge>();
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

}  // namespace

std::unique_ptr<ScreenReaderBridge> make_screen_reader_bridge() {
    return std::make_unique<SilentBridge>();
}

}  // namespace a11y

#endif
