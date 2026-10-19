/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "event_pipe.hpp"
#include "pc/pc.h"
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <thread>

namespace a11y {

namespace {

using std::chrono::milliseconds;
using std::chrono::steady_clock;

/* Between a press and its release, and after every event, so each lands in a
 * later turn of the loop than the one before it, as a real key's would. */
constexpr milliseconds kGap{50};
/* How long a pushed event may wait in SDL's queue: a scene load holds the
 * game loop for a few seconds at most. */
constexpr milliseconds kTakenTimeout{10000};
/* How long the first request waits for SDL to start. */
constexpr milliseconds kStartTimeout{30000};

bool all_digits(std::string_view s) {
    if (s.empty()) {
        return false;
    }
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

bool same_word(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
        char y = b[i] >= 'A' && b[i] <= 'Z' ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
        if (x != y) {
            return false;
        }
    }
    return true;
}

/* A press and its release, each a copy of event with its type set; the
 * caller marks the press as down, a field of each event kind's own. */
void press(PipeRequest& r, SDL_Event event, SDL_EventType down_type, SDL_EventType up_type) {
    PipeEvent e;
    e.event = event;
    e.event.type = down_type;
    r.events.push_back(e);
    e.event.type = up_type;
    r.events.push_back(e);
}

void parse_key(std::string_view arg, PipeRequest& r) {
    if (arg.empty()) {
        r.error = "nothing to press";
        return;
    }
    SDL_Keymod mod = SDL_KMOD_NONE;
    std::string_view name = arg;
    for (std::size_t plus; (plus = name.find('+')) != std::string_view::npos && plus > 0;) {
        const std::string_view modifier = name.substr(0, plus);
        if (same_word(modifier, "shift")) {
            mod |= SDL_KMOD_LSHIFT;
        } else if (same_word(modifier, "ctrl")) {
            mod |= SDL_KMOD_LCTRL;
        } else if (same_word(modifier, "alt")) {
            mod |= SDL_KMOD_LALT;
        } else if (same_word(modifier, "gui")) {
            mod |= SDL_KMOD_LGUI;
        } else {
            r.error = "unknown modifier \"" + std::string(modifier) + "\"";
            return;
        }
        name.remove_prefix(plus + 1);
    }
    const SDL_Scancode scancode = SDL_GetScancodeFromName(std::string(name).c_str());
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        r.error = "unknown key \"" + std::string(name) + "\"";
        return;
    }
    SDL_Event event{};
    event.key.scancode = scancode;
    /* RmlUi reads the keycode, vi.c's F1 the scancode, so both are set, the
     * keycode from the keyboard layout as SDL's own events take it. SDL calls
     * this not thread safe: it reads the layout, which only the main thread
     * changes, and only when the player switches layouts. */
    event.key.key = SDL_GetKeyFromScancode(scancode, mod, true);
    event.key.mod = mod;
    press(r, event, SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP);
    r.events[0].event.key.down = true;
}

void parse_text(std::string_view arg, PipeRequest& r) {
    if (arg.empty()) {
        r.error = "nothing to type";
        return;
    }
    r.what = "text \"" + std::string(arg) + "\"";
    /* One event a character, as typing sends them; a character is one to
     * four bytes of UTF-8, told by its first byte. */
    for (std::size_t i = 0; i < arg.size();) {
        const auto lead = static_cast<unsigned char>(arg[i]);
        std::size_t length = lead < 0xc0 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        length = std::min(length, arg.size() - i);
        PipeEvent e;
        e.event.type = SDL_EVENT_TEXT_INPUT;
        e.text = std::string(arg.substr(i, length));
        r.events.push_back(e);
        i += length;
    }
}

void parse_gamepad(std::string_view arg, PipeRequest& r) {
    if (arg.empty()) {
        r.error = "nothing to press";
        return;
    }
    const SDL_GamepadButton button = SDL_GetGamepadButtonFromString(std::string(arg).c_str());
    if (button == SDL_GAMEPAD_BUTTON_INVALID) {
        r.error = "unknown gamepad button \"" + std::string(arg) + "\"";
        return;
    }
    r.what = std::string("gamepad ") + SDL_GetGamepadStringForButton(button);
    SDL_Event event{};
    /* No device: SDL never gives a joystick the id 0, so aurora's input
     * router, which looks the device up, drops the event before RmlUi, which
     * reads no gamepad anyway. The launcher and the port menu read only the
     * button. */
    event.gbutton.which = 0;
    event.gbutton.button = static_cast<Uint8>(button);
    press(r, event, SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_EVENT_GAMEPAD_BUTTON_UP);
    r.events[0].event.gbutton.down = true;
}

/* The event type of the marker pushed behind each event, registered once. */
Uint32 s_marker = 0;
/* Every typed character, kept for the run: SDL copies a pushed event but not
 * the text it points at, and aurora hands the raw event on to the loop after
 * routing it, so the text must outlive the turn that took it. A few bytes a
 * character, in driven runs only. Never freed: the game may exit while the
 * thread pushes, and a destructor would free the text under it. */
std::deque<std::string>& s_texts = *new std::deque<std::string>;

/* Whether the marker numbered seq is still in SDL's queue, or the queue
 * cannot be read (SDL shutting down): not taken either way. */
bool marker_queued(Sint32 seq) {
    SDL_Event queued[32];
    const int n = SDL_PeepEvents(queued, 32, SDL_PEEKEVENT, s_marker, s_marker);
    if (n < 0) {
        return true;
    }
    for (int i = 0; i < n; ++i) {
        if (queued[i].user.code == seq) {
            return true;
        }
    }
    return false;
}

/* Waits for SDL's event queue to exist, and registers the marker; the empty
 * string once ready, or why not. */
std::string ready() {
    if (s_marker != 0) {
        return "";
    }
    const auto deadline = steady_clock::now() + kStartTimeout;
    while (!SDL_WasInit(SDL_INIT_EVENTS)) {
        if (steady_clock::now() > deadline) {
            return "not taken: SDL never started";
        }
        std::this_thread::sleep_for(milliseconds(10));
    }
    s_marker = SDL_RegisterEvents(1);
    return s_marker != 0 ? "" : std::string("refused: ") + SDL_GetError();
}

/* Pushes one event and waits for the loop to take it. The marker pushed
 * behind it leaves SDL's queue only when a turn of the loop polls the queue
 * past the event, which aurora_update does whenever the game's frame
 * boundary or the launcher's loop runs, also while the port menu holds the
 * game still. The empty string when taken, or why not. */
std::string push(const PipeEvent& e) {
    static Sint32 seq = 0;
    SDL_Event event = e.event;
    if (event.type == SDL_EVENT_TEXT_INPUT) {
        s_texts.push_back(e.text);
        event.text.text = s_texts.back().c_str();
    }
    if (!SDL_PushEvent(&event)) {
        return std::string("refused: ") + SDL_GetError();
    }
    SDL_Event marker{};
    marker.type = s_marker;
    marker.user.code = ++seq;
    if (!SDL_PushEvent(&marker)) {
        return std::string("refused: ") + SDL_GetError();
    }
    const auto deadline = steady_clock::now() + kTakenTimeout;
    while (marker_queued(seq)) {
        if (steady_clock::now() > deadline) {
            /* Left behind, the marker would crowd later ones out of the
             * peek; it is the only one in flight, so every marker goes. */
            SDL_FlushEvent(s_marker);
            return "not taken in " + std::to_string(kTakenTimeout.count() / 1000) + " s";
        }
        std::this_thread::sleep_for(milliseconds(2));
    }
    std::this_thread::sleep_for(kGap);
    return "";
}

void serve(const PipeRequest& r) {
    std::string failure;
    for (std::size_t i = 0; failure.empty() && i < r.events.size(); ++i) {
        failure = push(r.events[i]);
    }
    if (failure.empty()) {
        pc_log_line("[a11y] event %s: %s taken", r.id.c_str(), r.what.c_str());
    } else {
        pc_log_line("[a11y] event %s: %s %s", r.id.c_str(), r.what.c_str(), failure.c_str());
    }
}

/* The next line of f, however long, without its line ending; false at the
 * end of the file. */
bool read_line(FILE* f, std::string& line) {
    line.clear();
    int c;
    while ((c = std::fgetc(f)) != EOF && c != '\n') {
        line.push_back(static_cast<char>(c));
    }
    if (c == EOF && line.empty()) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

/* Reads the pipe a line at a time, as keyboard.c's MELEE_KEY_FIFO thread
 * does, and reopens it when the writer goes away. */
void pipe_thread(std::string path) {
    std::string line;
    for (;;) {
        FILE* f = std::fopen(path.c_str(), "r"); /* waits for a writer */
        if (f == nullptr) {
            std::this_thread::sleep_for(milliseconds(200));
            continue;
        }
        while (read_line(f, line)) {
            /* SDL first: parsing a key reads SDL's keymap. */
            const std::string not_ready = ready();
            const PipeRequest r = parse_pipe_line(line);
            if (r.id.empty()) {
                pc_log_line("[a11y] event pipe: line not understood: %s", line.c_str());
            } else if (!not_ready.empty()) {
                pc_log_line(
                    "[a11y] event %s: %s %s", r.id.c_str(), r.what.c_str(), not_ready.c_str());
            } else if (!r.error.empty()) {
                pc_log_line("[a11y] event %s: refused: %s", r.id.c_str(), r.error.c_str());
            } else {
                serve(r);
            }
        }
        std::fclose(f);
    }
}

}  // namespace

PipeRequest parse_pipe_line(std::string_view line) {
    PipeRequest r;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.remove_suffix(1);
    }
    const std::size_t space = line.find(' ');
    const std::string_view id = line.substr(0, space);
    if (!all_digits(id)) {
        return r;
    }
    r.id = std::string(id);
    const std::string_view rest = space == std::string_view::npos ? "" : line.substr(space + 1);
    const std::size_t verb_end = rest.find(' ');
    const std::string_view verb = rest.substr(0, verb_end);
    const std::string_view arg =
        verb_end == std::string_view::npos ? "" : rest.substr(verb_end + 1);
    r.what = std::string(rest);
    if (verb == "key") {
        parse_key(arg, r);
    } else if (verb == "text") {
        parse_text(arg, r);
    } else if (verb == "gamepad") {
        parse_gamepad(arg, r);
    } else {
        r.error = "unknown request \"" + std::string(verb) + "\"";
    }
    if (!r.error.empty()) {
        r.events.clear();
    }
    return r;
}

void event_pipe_start() {
    static bool started = false;
    const char* path = std::getenv("MELEE_A11Y_EVENT_FIFO");
    if (started || path == nullptr || path[0] == '\0') {
        return;
    }
    started = true;
    pc_log_line("[a11y] event pipe: %s", path);
    /* Detached: it waits in the pipe for the whole run, and exit ends it. */
    std::thread(pipe_thread, std::string(path)).detach();
}

}  // namespace a11y
