/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "css_speech.hpp"
#include "css_names.hpp"
#include "pc/pc.h"
#include "speech.hpp"
#include <optional>
#include <string_view>

namespace a11y {

void CssSpeech::frame(const A11yCssState& state) {
    bool arrived = !m_seen;
    A11yCssState last = m_last;
    m_seen = true;
    m_last = state;
    if (arrived) {
        m_speech.announce(opening(state), Mode::interrupt);
        return;
    }
    /* Single-player modes have one hand and their own targets; until they are
     * read, the opening is all they say. */
    if (state.leaving || state.hand_count != A11Y_CSS_SLOTS) {
        return;
    }
    std::string text = change_announcement(last, state);
    if (!text.empty()) {
        m_speech.announce(text, Mode::interrupt);
    }
}

std::string CssSpeech::opening(const A11yCssState& state) {
    /* A character counts once its coin rests on a portrait: a slot not yet
     * opened, or one whose coin went back, may keep a stale one. */
    const A11yCssSlot& slot = state.slots[state.local_slot];
    bool chosen =
        slot.kind != A11Y_CSS_CLOSED && slot.portrait >= 0 && slot.character != A11Y_NO_CHARACTER;
    return "Character select. Player " + std::to_string(state.local_player + 1) + ", " +
           (chosen ? name(slot.character) : "no character") + ".";
}

std::string CssSpeech::change_announcement(const A11yCssState& last, const A11yCssState& now) {
    int was_carrying = last.hand.coin;
    int carrying = now.hand.coin;
    if (carrying >= 0 && carrying != was_carrying) {
        /* The frame a coin is picked up still shows where it lay; where it
         * is now, under the hand, shows a frame later. Speaking at once
         * would have that portrait cut the pickup off. */
        m_pickup_pending = true;
        return "";
    }
    bool pickup_pending = m_pickup_pending;
    m_pickup_pending = false;
    if (pickup_pending && carrying >= 0) {
        return holding(now, carrying);
    }
    if (was_carrying >= 0 && carrying < 0) {
        return dropped(last, now, was_carrying);
    }
    /* X and Y change the costume of the carried coin, or of one's own. */
    int slot = carrying >= 0 ? carrying : now.local_slot;
    const A11yCssSlot& before = last.slots[slot];
    const A11yCssSlot& after = now.slots[slot];
    if (carrying >= 0 && after.over_portrait != before.over_portrait) {
        /* Leaving every portrait says nothing. Entering one also resets the
         * costume, which goes unsaid. */
        return after.over_portrait >= 0 ? name(now.portraits[after.over_portrait].character) : "";
    }
    if (after.over_portrait >= 0 && after.over_portrait == before.over_portrait &&
        after.portrait == before.portrait && after.character == before.character &&
        after.costume != before.costume)
    {
        return "Costume " + std::to_string(after.costume + 1);
    }
    return "";
}

std::string CssSpeech::holding(const A11yCssState& now, int coin) {
    std::string out = coin == now.local_slot ?
                          "Holding your coin" :
                          "Holding player " + std::to_string(coin + 1) + "'s coin";
    int over = now.slots[coin].over_portrait;
    if (over >= 0) {
        out += ". " + name(now.portraits[over].character);
    }
    return out;
}

std::string CssSpeech::dropped(const A11yCssState& last, const A11yCssState& now, int coin) {
    const A11yCssSlot& before = last.slots[coin];
    const A11yCssSlot& after = now.slots[coin];
    /* Carried down into the player slots. */
    if (after.portrait < 0 || after.character == A11Y_NO_CHARACTER) {
        return "No character";
    }
    /* B puts the coin back on the character chosen before, which A cannot
     * do: A chooses the portrait under the coin, and a different portrait is
     * a different character. */
    if (after.character == before.character && after.portrait != before.over_portrait) {
        return "Back to " + name(after.character);
    }
    /* A choice: the game's announcer names the character. */
    return "";
}

std::string CssSpeech::name(A11yCharacter character) {
    if (std::optional<std::string_view> known = character_name(character)) {
        return std::string(*known);
    }
    if (m_logged_missing.insert(character).second && log_enabled()) {
        pc_log_line("[a11y] character %d: not in the character names table", character);
    }
    return "Unknown character " + std::to_string(character);
}

}  // namespace a11y
