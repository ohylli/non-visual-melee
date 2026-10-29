/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Memory card prompt speech: reads the memory card scene's prompts aloud
 * in the game's own words (.scratch/memory-card/spec.md), from two hooks:
 * the prompt the scene shows, by its kind and text number, and each frame
 * of the scene, with the choice hovered.
 *
 * A prompt is said once it has stayed up through a whole frame of the
 * scene, so the one-frame "Erasing..." and "Creating..." stay silent and
 * the default the game moves just after showing a prompt is the choice
 * said. A prompt with a choice says its text, the hovered choice and, on
 * the first such prompt of the game run, the keys; a message says its text
 * and the key. The first prompt of the run waits behind what is being
 * said; every later prompt and every move of the choice interrupts. The
 * reader touches no game state. */
#pragma once
#include "game_text.hpp"
#include "speech.hpp"
#include <cstdint>
#include <string>

namespace a11y {

/* The message window's strings (SdMsgBox) by the number the game's code
 * uses; nullptr for one it cannot find. */
using MemcardStrings = const std::uint8_t* (*)(int number);

/* gm_801AEE6C's kinds of prompt. */
enum class MemcardKind {
    message = 0,  /* no choice; A or Start continues */
    yes_no = 1,   /* Yes on the left, No on the right */
    ok_retry = 2, /* OK on the left, Try Again on the right */
};

class MemcardSpeech {
public:
    /* speech must outlive this; text decodes the game's text, strings
     * finds it. */
    MemcardSpeech(Speech& speech, GameTextSource text, MemcardStrings strings)
        : m_speech(speech), m_text(text), m_strings(strings) {}

    /* The memory card scene was entered: no prompt is up yet. */
    void visit_started();

    /* The scene shows a prompt of a kind (gm_801AEE6C's first argument)
     * with the text numbered text_id. Said by a later frame. */
    void prompt(int kind, int text_id);

    /* The start of a frame of the scene, the choice hovered then: 0 the
     * left one, 1 the right one. */
    void frame(int hovered_choice);

private:
    void say_prompt(int hovered_choice);
    /* A choice's name, 0 left and 1 right, in the game's words. */
    std::string choice_name(int choice);
    /* String number of the game's text made speakable, its trouble logged
     * once; empty for none. */
    std::string game_words(int number);

    Speech& m_speech;
    GameTextSource m_text;
    MemcardStrings m_strings;
    /* The prompt last shown, and whether one is up in this visit. */
    bool m_shown = false;
    MemcardKind m_kind = MemcardKind::message;
    int m_text_id = 0;
    /* The frames that have started since it was shown. */
    int m_frames = 0;
    /* It has been said, with this choice hovered. */
    bool m_said = false;
    int m_choice = 0;
    /* No prompt with a choice has said the keys yet this game run. */
    bool m_keys_due = true;
    /* No prompt has been said yet this game run. */
    bool m_first = true;
    LogOnce m_log_once;
};

}  // namespace a11y
