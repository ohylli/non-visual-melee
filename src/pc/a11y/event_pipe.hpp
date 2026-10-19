/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The event pipe: a test driver that pushes real SDL events into the game, so
 * a drive script reaches the pages that react to SDL events (the launcher and
 * the port menu), which MELEE_KEY_FIFO never sends any to. Started only when
 * MELEE_A11Y_EVENT_FIFO names a pipe; a fork thread reads requests from it,
 * one a line:
 *
 *   ID key [MOD+...]NAME    a key pressed and released: an SDL key name
 *                           ("F1", "Return", "Escape", "Down"), modifiers
 *                           Shift, Ctrl, Alt and Gui
 *   ID text TEXT            the rest of the line typed, a character at a time
 *   ID gamepad BUTTON       a gamepad button pressed and released, by SDL's
 *                           name ("back", "a", "b", "dpdown")
 *
 * ID is the driver's number for the request, echoed in the log line that
 * answers it: "[a11y] event ID: WHAT taken" once the loop running (the
 * game's frame boundary or the launcher's loop) has taken every event of it
 * from SDL's queue, which works while the game stands still under the port
 * menu, or "refused: WHY" or "not taken". docs/a11y/rmlui-screens.md has how
 * the drivers use it. */
#pragma once
#include <SDL3/SDL_events.h>
#include <string>
#include <string_view>
#include <vector>

namespace a11y {

/* One event to push. text holds a typed character, which the event points
 * at only once it is pushed. */
struct PipeEvent {
    SDL_Event event{};
    std::string text;
};

/* One line of the pipe, parsed: its id (empty for a line with none), what it
 * asks for as the log names it, and its events in order, or why it was
 * refused. */
struct PipeRequest {
    std::string id;
    std::string what;
    std::vector<PipeEvent> events;
    std::string error;
};

PipeRequest parse_pipe_line(std::string_view line);

/* Starts the pipe's thread when MELEE_A11Y_EVENT_FIFO is set; once. */
void event_pipe_start();

}  // namespace a11y
