/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <memory>
#include <string>
#include <string_view>

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

}  // namespace a11y
