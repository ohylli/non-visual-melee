/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The event pipe's lines turned into the SDL events it pushes. Only the
 * parsing is tested: pushing needs a running game loop to take the events,
 * which the drive runs check. Links event_pipe.cpp and SDL, so this file
 * supplies the pc_log_line that main.c normally provides. */
#include "event_pipe.hpp"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <cassert>
#include <iostream>
#include <string>

namespace {

void key_is_a_press_and_a_release() {
    const a11y::PipeRequest r = a11y::parse_pipe_line("3 key F1\n");
    assert(r.error.empty());
    assert(r.id == "3");
    assert(r.what == "key F1");
    assert(r.events.size() == 2);
    const SDL_KeyboardEvent& down = r.events[0].event.key;
    const SDL_KeyboardEvent& up = r.events[1].event.key;
    assert(down.type == SDL_EVENT_KEY_DOWN && down.down && !down.repeat);
    assert(up.type == SDL_EVENT_KEY_UP && !up.down);
    /* vi.c tests the scancode for F1, RmlUi the keycode for everything. */
    assert(down.scancode == SDL_SCANCODE_F1 && up.scancode == SDL_SCANCODE_F1);
    assert(down.key == SDLK_F1 && up.key == SDLK_F1);
    assert(down.mod == SDL_KMOD_NONE);
    /* No window: aurora takes an event without one as its own window's. */
    assert(down.windowID == 0);
}

void key_names_ignore_case_and_take_modifiers() {
    const a11y::PipeRequest r = a11y::parse_pipe_line("4 key shift+TAB");
    assert(r.error.empty());
    assert(r.events.size() == 2);
    assert(r.events[0].event.key.scancode == SDL_SCANCODE_TAB);
    assert(r.events[0].event.key.key == SDLK_TAB);
    assert(r.events[0].event.key.mod == SDL_KMOD_LSHIFT);
    assert(r.events[1].event.key.mod == SDL_KMOD_LSHIFT);

    const a11y::PipeRequest both = a11y::parse_pipe_line("5 key Ctrl+Alt+Return");
    assert(both.error.empty());
    assert(both.events[0].event.key.scancode == SDL_SCANCODE_RETURN);
    assert(both.events[0].event.key.mod == (SDL_KMOD_LCTRL | SDL_KMOD_LALT));
}

void unknown_key_is_refused() {
    const a11y::PipeRequest r = a11y::parse_pipe_line("6 key Enterr");
    assert(r.id == "6");
    assert(r.events.empty());
    assert(r.error == "unknown key \"Enterr\"");
    assert(a11y::parse_pipe_line("7 key Hyper+Tab").error == "unknown modifier \"Hyper\"");
    assert(a11y::parse_pipe_line("8 key").error == "nothing to press");
}

void text_is_one_event_a_character() {
    const a11y::PipeRequest r = a11y::parse_pipe_line("9 text Ab #1\r\n");
    assert(r.error.empty());
    assert(r.what == "text \"Ab #1\"");
    assert(r.events.size() == 5);
    for (const a11y::PipeEvent& e : r.events) {
        assert(e.event.type == SDL_EVENT_TEXT_INPUT);
    }
    assert(r.events[0].text == "A");
    assert(r.events[2].text == " ");
    assert(r.events[4].text == "1");
}

void text_keeps_each_character_whole() {
    /* "ä" and "€", two and three bytes of UTF-8. */
    const a11y::PipeRequest r = a11y::parse_pipe_line("10 text \xc3\xa4\xe2\x82\xac");
    assert(r.error.empty());
    assert(r.events.size() == 2);
    assert(r.events[0].text == "\xc3\xa4");
    assert(r.events[1].text == "\xe2\x82\xac");
    assert(a11y::parse_pipe_line("11 text").error == "nothing to type");
}

void gamepad_button_is_a_press_and_a_release() {
    const a11y::PipeRequest r = a11y::parse_pipe_line("12 gamepad Back");
    assert(r.error.empty());
    assert(r.what == "gamepad back");
    assert(r.events.size() == 2);
    const SDL_GamepadButtonEvent& down = r.events[0].event.gbutton;
    const SDL_GamepadButtonEvent& up = r.events[1].event.gbutton;
    assert(down.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && down.down);
    assert(up.type == SDL_EVENT_GAMEPAD_BUTTON_UP && !up.down);
    assert(down.button == SDL_GAMEPAD_BUTTON_BACK && up.button == SDL_GAMEPAD_BUTTON_BACK);
    /* No device: SDL never gives a joystick the id 0. */
    assert(down.which == 0);
    /* SDL's names: a is the bottom face button, b the right one. */
    assert(a11y::parse_pipe_line("13 gamepad b").events[0].event.gbutton.button ==
           SDL_GAMEPAD_BUTTON_EAST);
    assert(a11y::parse_pipe_line("14 gamepad dpdown").events[0].event.gbutton.button ==
           SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    assert(a11y::parse_pipe_line("15 gamepad Select").error == "unknown gamepad button \"Select\"");
}

void lines_without_a_request_are_refused() {
    const a11y::PipeRequest r = a11y::parse_pipe_line("16 press F1");
    assert(r.id == "16");
    assert(r.error == "unknown request \"press\"");
    assert(a11y::parse_pipe_line("F1").id.empty());
    assert(a11y::parse_pipe_line("").id.empty());
}

}  // namespace

extern "C" void pc_log_line(const char*, ...) {}

int main() {
    key_is_a_press_and_a_release();
    key_names_ignore_case_and_take_modifiers();
    unknown_key_is_refused();
    text_is_one_event_a_character();
    text_keeps_each_character_whole();
    gamepad_button_is_a_press_and_a_release();
    lines_without_a_request_are_refused();
    std::cout << "event_pipe: all tests passed\n";
    return 0;
}
